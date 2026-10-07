#include "AtlasGenerator.h"
#include <msdfgen/msdfgen.h>
#include <msdfgen/core/ShapeDistanceFinder.h>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstring>
#include <fstream>
#include <memory>
#include <thread>

#pragma warning(push, 0)
#pragma warning(disable : 4244 4267 4305 4996)
#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#include <stb/stb_truetype.h>
#define NANOSVG_IMPLEMENTATION
#include <nanosvg/nanosvg.h>
#define NANOSVGRAST_IMPLEMENTATION
#include <nanosvg/nanosvgrast.h>
#pragma warning(pop)

namespace Game
{
	namespace
	{
		constexpr int CellGutter = 1;
		constexpr double ColoringAngle = 3.0;
		constexpr int Supersampling = 8;
		constexpr float Unreachable = 1e20f;

		struct SvgDeleter
		{
			void operator()(NSVGimage* image) const { nsvgDelete(image); }
		};

		struct Job
		{
			msdfgen::Shape Shape;
			double Scale = 1.0;
			msdfgen::Vector2 Translate;
			int Width = 0;
			int Height = 0;
			int X = 0;
			int Y = 0;
			glm::vec2* UvMin = nullptr;
			glm::vec2* UvMax = nullptr;
			std::unique_ptr<NSVGimage, SvgDeleter> Svg;
			double Padding = 0.0;
			double Frame = 512.0;
		};

		std::vector<uint8_t> ReadFile(const std::string& path)
		{
			std::ifstream file(path, std::ios::binary | std::ios::ate);
			if (!file)
				return {};
			std::vector<uint8_t> data(size_t(file.tellg()));
			file.seekg(0);
			file.read(reinterpret_cast<char*>(data.data()), std::streamsize(data.size()));
			return file ? data : std::vector<uint8_t>{};
		}

		bool FixOrientation(msdfgen::Shape& shape)
		{
			shape.normalize();
			if (shape.contours.empty() || !shape.validate())
				return false;
			const msdfgen::Shape::Bounds bounds = shape.getBounds();
			const msdfgen::Point2 outside(bounds.l - (bounds.r - bounds.l) - 1.0, bounds.b - (bounds.t - bounds.b) - 1.0);
			if (msdfgen::SimpleTrueShapeDistanceFinder::oneShotDistance(shape, outside) > 0.0)
				for (msdfgen::Contour& contour : shape.contours)
					contour.reverse();
			return true;
		}

		void CloseContour(msdfgen::Contour& contour, msdfgen::Point2 start, msdfgen::Point2 last)
		{
			if (!contour.edges.empty() && (start - last).length() > 1e-9)
				contour.addEdge(msdfgen::EdgeHolder(last, start));
		}

		msdfgen::Shape GlyphShape(const stbtt_fontinfo& info, int glyph)
		{
			msdfgen::Shape shape;
			stbtt_vertex* vertices = nullptr;
			const int count = stbtt_GetGlyphShape(&info, glyph, &vertices);
			msdfgen::Contour* contour = nullptr;
			msdfgen::Point2 start;
			msdfgen::Point2 last;
			for (int i = 0; i < count; i++)
			{
				const stbtt_vertex& vertex = vertices[i];
				const msdfgen::Point2 point(vertex.x, vertex.y);
				switch (vertex.type)
				{
				case STBTT_vmove:
					if (contour)
						CloseContour(*contour, start, last);
					contour = &shape.addContour();
					start = last = point;
					break;
				case STBTT_vline:
					if (contour && (point - last).length() > 1e-9)
						contour->addEdge(msdfgen::EdgeHolder(last, point));
					last = point;
					break;
				case STBTT_vcurve:
					if (contour && (point - last).length() > 1e-9)
						contour->addEdge(msdfgen::EdgeHolder(last, msdfgen::Point2(vertex.cx, vertex.cy), point));
					last = point;
					break;
				case STBTT_vcubic:
					if (contour && (point - last).length() > 1e-9)
						contour->addEdge(msdfgen::EdgeHolder(last, msdfgen::Point2(vertex.cx, vertex.cy), msdfgen::Point2(vertex.cx1, vertex.cy1), point));
					last = point;
					break;
				default:
					break;
				}
			}
			if (contour)
				CloseContour(*contour, start, last);
			stbtt_FreeShape(&info, vertices);
			std::erase_if(shape.contours, [](const msdfgen::Contour& item) { return item.edges.empty(); });
			return shape;
		}

		bool Collinear(msdfgen::Point2 a, msdfgen::Point2 b, msdfgen::Point2 c, msdfgen::Point2 d)
		{
			const msdfgen::Vector2 direction = d - a;
			const double length = direction.length();
			if (length < 1e-9)
				return true;
			const auto offset = [&](msdfgen::Point2 point) { return std::abs(msdfgen::crossProduct(direction, point - a)) / length; };
			return offset(b) < 1e-3 && offset(c) < 1e-3;
		}

		msdfgen::Shape IconShape(const NSVGimage* image)
		{
			msdfgen::Shape shape;
			const double height = double(image->height);
			for (NSVGshape* svgShape = image->shapes; svgShape; svgShape = svgShape->next)
			{
				if (!(svgShape->flags & NSVG_FLAGS_VISIBLE) || svgShape->fill.type != NSVG_PAINT_COLOR)
					continue;
				const uint32_t color = svgShape->fill.color;
				if ((color & 0xFF) + ((color >> 8) & 0xFF) + ((color >> 16) & 0xFF) < 3 * 0x40)
					continue;
				msdfgen::Shape part;
				for (NSVGpath* svgPath = svgShape->paths; svgPath; svgPath = svgPath->next)
				{
					if (svgPath->npts < 4)
						continue;
					msdfgen::Contour& contour = part.addContour();
					const auto point = [&](int index) { return msdfgen::Point2(svgPath->pts[index * 2], height - svgPath->pts[index * 2 + 1]); };
					const msdfgen::Point2 start = point(0);
					msdfgen::Point2 last = start;
					for (int i = 0; i + 3 < svgPath->npts; i += 3)
					{
						const msdfgen::Point2 c1 = point(i + 1);
						const msdfgen::Point2 c2 = point(i + 2);
						const msdfgen::Point2 end = point(i + 3);
						if ((end - last).length() < 1e-6 && (c1 - last).length() < 1e-6 && (c2 - last).length() < 1e-6)
							continue;
						if (Collinear(last, c1, c2, end))
						{
							if ((end - last).length() > 1e-6)
								contour.addEdge(msdfgen::EdgeHolder(last, end));
						}
						else
							contour.addEdge(msdfgen::EdgeHolder(last, c1, c2, end));
						last = end;
					}
					CloseContour(contour, start, last);
				}
				std::erase_if(part.contours, [](const msdfgen::Contour& item) { return item.edges.empty(); });
				if (svgShape->fillRule == NSVG_FILLRULE_EVENODD)
					part.orientContours();
				for (msdfgen::Contour& contour : part.contours)
					shape.addContour(std::move(contour));
			}
			return shape;
		}

		std::vector<uint8_t> RasterizeIcon(const Job& job, int factor)
		{
			const int width = job.Width * factor;
			const int height = job.Height * factor;
			std::vector<uint8_t> pixels(size_t(width) * height * 4, 0);
			NSVGrasterizer* rasterizer = nsvgCreateRasterizer();
			const float scale = float(job.Scale) * float(factor);
			const float offsetX = float(job.Padding) * float(factor);
			const float offsetY = float(double(job.Height) - job.Frame * job.Scale - job.Padding) * float(factor);
			nsvgRasterize(rasterizer, job.Svg.get(), offsetX, offsetY, scale, pixels.data(), width, height, width * 4);
			nsvgDeleteRasterizer(rasterizer);
			std::vector<uint8_t> coverage(size_t(width) * height);
			for (size_t i = 0; i < coverage.size(); i++)
				coverage[i] = pixels[i * 4];
			return coverage;
		}

		void DistanceTransform(std::vector<float>& grid, int width, int height)
		{
			const size_t length = size_t(std::max(width, height));
			std::vector<float> line(length);
			std::vector<float> result(length);
			std::vector<int> parabolas(length);
			std::vector<float> bounds(length + 1);
			const auto intersection = [&](int q, int p) { return ((line[size_t(q)] + float(q * q)) - (line[size_t(p)] + float(p * p))) / float(2 * q - 2 * p); };
			const auto pass = [&](int count)
			{
				int k = 0;
				parabolas[0] = 0;
				bounds[0] = -Unreachable;
				bounds[1] = Unreachable;
				for (int q = 1; q < count; q++)
				{
					float s = intersection(q, parabolas[size_t(k)]);
					while (s <= bounds[size_t(k)])
						s = intersection(q, parabolas[size_t(--k)]);
					k++;
					parabolas[size_t(k)] = q;
					bounds[size_t(k)] = s;
					bounds[size_t(k) + 1] = Unreachable;
				}
				k = 0;
				for (int q = 0; q < count; q++)
				{
					while (bounds[size_t(k) + 1] < float(q))
						k++;
					const int p = parabolas[size_t(k)];
					result[size_t(q)] = float((q - p) * (q - p)) + line[size_t(p)];
				}
			};
			for (int x = 0; x < width; x++)
			{
				for (int y = 0; y < height; y++)
					line[size_t(y)] = grid[size_t(y) * width + x];
				pass(height);
				for (int y = 0; y < height; y++)
					grid[size_t(y) * width + x] = result[size_t(y)];
			}
			for (int y = 0; y < height; y++)
			{
				std::copy_n(grid.begin() + ptrdiff_t(y) * width, width, line.begin());
				pass(width);
				std::copy_n(result.begin(), width, grid.begin() + ptrdiff_t(y) * width);
			}
		}

		bool MatchesRaster(const Job& job, const std::vector<uint8_t>& cell)
		{
			const std::vector<uint8_t> coverage = RasterizeIcon(job, 1);
			uint32_t mismatches = 0;
			for (size_t i = 0; i < coverage.size(); i++)
			{
				const uint8_t* pixel = &cell[i * 4];
				const uint8_t median = std::max(std::min(pixel[0], pixel[1]), std::min(std::max(pixel[0], pixel[1]), pixel[2]));
				const bool inside = median > 127;
				if ((coverage[i] > 204 && !inside) || (coverage[i] < 51 && inside))
					mismatches++;
			}
			return mismatches <= std::max<uint32_t>(6, uint32_t(coverage.size() / 200));
		}

		void FillFromRaster(const Job& job, std::vector<uint8_t>& cell, double distanceRange)
		{
			const int width = job.Width * Supersampling;
			const int height = job.Height * Supersampling;
			const std::vector<uint8_t> coverage = RasterizeIcon(job, Supersampling);
			std::vector<float> toInside(coverage.size());
			std::vector<float> toOutside(coverage.size());
			for (size_t i = 0; i < coverage.size(); i++)
			{
				const bool inside = coverage[i] >= 128;
				toInside[i] = inside ? 0.0f : Unreachable;
				toOutside[i] = inside ? Unreachable : 0.0f;
			}
			DistanceTransform(toInside, width, height);
			DistanceTransform(toOutside, width, height);
			const auto signedDistance = [&](int x, int y)
			{
				x = std::clamp(x, 0, width - 1);
				y = std::clamp(y, 0, height - 1);
				const size_t index = size_t(y) * width + x;
				return toOutside[index] > 0.0f ? std::sqrt(toOutside[index]) - 0.5f : 0.5f - std::sqrt(toInside[index]);
			};
			for (int y = 0; y < job.Height; y++)
				for (int x = 0; x < job.Width; x++)
				{
					const float centerX = (float(x) + 0.5f) * Supersampling - 0.5f;
					const float centerY = (float(y) + 0.5f) * Supersampling - 0.5f;
					const int baseX = int(std::floor(centerX));
					const int baseY = int(std::floor(centerY));
					const float fx = centerX - float(baseX);
					const float fy = centerY - float(baseY);
					const float top = signedDistance(baseX, baseY) * (1.0f - fx) + signedDistance(baseX + 1, baseY) * fx;
					const float bottom = signedDistance(baseX, baseY + 1) * (1.0f - fx) + signedDistance(baseX + 1, baseY + 1) * fx;
					const float distance = (top * (1.0f - fy) + bottom * fy) / float(Supersampling);
					const uint8_t value = uint8_t(std::clamp((0.5f + distance / float(distanceRange)) * 255.0f + 0.5f, 0.0f, 255.0f));
					uint8_t* pixel = &cell[(size_t(y) * job.Width + x) * 4];
					pixel[0] = pixel[1] = pixel[2] = pixel[3] = value;
				}
		}

		void PlaceJobs(std::vector<Job*>& jobs, uint32_t width, uint32_t& height)
		{
			std::sort(jobs.begin(), jobs.end(), [](const Job* a, const Job* b) { return a->Height != b->Height ? a->Height > b->Height : a->Width > b->Width; });
			int x = CellGutter;
			int y = CellGutter;
			int shelf = 0;
			for (Job* job : jobs)
			{
				if (x + job->Width + CellGutter > int(width))
				{
					x = CellGutter;
					y += shelf + CellGutter;
					shelf = 0;
				}
				job->X = x;
				job->Y = y;
				x += job->Width + CellGutter;
				shelf = std::max(shelf, job->Height);
			}
			height = uint32_t((y + shelf + CellGutter + 3) / 4 * 4);
		}

		void Generate(Job& job, VectorAtlas& atlas, double distanceRange, std::atomic<uint32_t>& fallbacks)
		{
			std::vector<uint8_t> cell(size_t(job.Width) * job.Height * 4);
			if (!job.Shape.contours.empty())
			{
				msdfgen::edgeColoringInkTrap(job.Shape, ColoringAngle);
				msdfgen::Bitmap<float, 4> bitmap(job.Width, job.Height);
				const msdfgen::SDFTransformation transformation(msdfgen::Projection(msdfgen::Vector2(job.Scale), job.Translate), msdfgen::DistanceMapping(msdfgen::Range(distanceRange / job.Scale)));
				msdfgen::generateMTSDF(bitmap, job.Shape, transformation, msdfgen::MSDFGeneratorConfig(true));
				for (int row = 0; row < job.Height; row++)
					for (int column = 0; column < job.Width; column++)
					{
						const float* source = bitmap(column, row);
						uint8_t* destination = &cell[(size_t(job.Height - 1 - row) * job.Width + column) * 4];
						for (int channel = 0; channel < 4; channel++)
							destination[channel] = uint8_t(std::clamp(source[channel] * 255.0f + 0.5f, 0.0f, 255.0f));
					}
			}
			if (job.Svg && (job.Shape.contours.empty() || !MatchesRaster(job, cell)))
			{
				FillFromRaster(job, cell, distanceRange);
				fallbacks++;
			}
			for (int row = 0; row < job.Height; row++)
				std::memcpy(atlas.Pixels.data() + (size_t(job.Y + row) * atlas.Width + size_t(job.X)) * 4, &cell[size_t(row) * job.Width * 4], size_t(job.Width) * 4);
		}
	}

	bool GenerateVectorAtlas(const VectorAtlasDesc& desc, VectorAtlas& atlas, std::string& error)
	{
		atlas = {};
		atlas.Width = desc.Width;
		atlas.DistanceRange = desc.DistanceRange;
		const double padding = double(desc.DistanceRange) * 0.5 + 1.0;
		std::vector<std::unique_ptr<Job>> jobs;

		atlas.Fonts.resize(desc.Fonts.size());
		for (size_t fontIndex = 0; fontIndex < desc.Fonts.size(); fontIndex++)
		{
			const FontSource& source = desc.Fonts[fontIndex];
			const std::vector<uint8_t> data = ReadFile(source.Path);
			stbtt_fontinfo info{};
			if (data.empty() || !stbtt_InitFont(&info, data.data(), stbtt_GetFontOffsetForIndex(data.data(), 0)))
			{
				error = "Can't load the font " + source.Path;
				return false;
			}
			FontFace& face = atlas.Fonts[fontIndex];
			const double emScale = stbtt_ScaleForMappingEmToPixels(&info, 1.0f);
			int ascent = 0;
			int descent = 0;
			int lineGap = 0;
			stbtt_GetFontVMetrics(&info, &ascent, &descent, &lineGap);
			face.Ascender = float(ascent * emScale);
			face.Descender = float(descent * emScale);
			face.LineGap = float(lineGap * emScale);
			const double scale = double(desc.GlyphSize) * emScale;

			std::vector<std::pair<uint32_t, int>> present;
			for (const auto& [first, last] : source.Ranges)
				for (uint32_t codepoint = first; codepoint <= last; codepoint++)
				{
					const int glyph = stbtt_FindGlyphIndex(&info, int(codepoint));
					if (glyph == 0)
						continue;
					present.emplace_back(codepoint, glyph);
					int advance = 0;
					int bearing = 0;
					stbtt_GetGlyphHMetrics(&info, glyph, &advance, &bearing);
					AtlasGlyph& entry = face.Glyphs[codepoint];
					entry.Advance = float(advance * emScale);

					auto job = std::make_unique<Job>();
					job->Shape = GlyphShape(info, glyph);
					if (!FixOrientation(job->Shape))
						continue;
					const msdfgen::Shape::Bounds bounds = job->Shape.getBounds();
					job->Scale = scale;
					job->Width = int(std::ceil((bounds.r - bounds.l) * scale + 2.0 * padding));
					job->Height = int(std::ceil((bounds.t - bounds.b) * scale + 2.0 * padding));
					job->Translate = msdfgen::Vector2(-bounds.l + padding / scale, -bounds.b + padding / scale);
					entry.Visible = true;
					entry.PlaneMin = glm::vec2(float(-job->Translate.x * emScale), float(-(job->Height / scale - job->Translate.y) * emScale));
					entry.PlaneMax = glm::vec2(float((job->Width / scale - job->Translate.x) * emScale), float(job->Translate.y * emScale));
					job->UvMin = &entry.UvMin;
					job->UvMax = &entry.UvMax;
					jobs.push_back(std::move(job));
				}
			for (const auto& [left, leftGlyph] : present)
				for (const auto& [right, rightGlyph] : present)
					if (const int kern = stbtt_GetGlyphKernAdvance(&info, leftGlyph, rightGlyph); kern != 0)
						face.Kerning[KerningPair(left, right)] = float(kern * emScale);
		}

		atlas.Icons.resize(desc.Icons.size());
		for (size_t i = 0; i < desc.Icons.size(); i++)
		{
			auto job = std::make_unique<Job>();
			job->Svg.reset(nsvgParseFromFile(desc.Icons[i].c_str(), "px", 96.0f));
			if (!job->Svg || job->Svg->width <= 0.0f || job->Svg->height <= 0.0f)
				continue;
			job->Shape = IconShape(job->Svg.get());
			if (!FixOrientation(job->Shape))
				job->Shape.contours.clear();
			job->Frame = std::max(double(job->Svg->width), double(job->Svg->height));
			job->Padding = padding;
			const double scale = double(desc.IconSize) / job->Frame;
			job->Scale = scale;
			job->Width = job->Height = int(std::ceil(double(desc.IconSize) + 2.0 * padding));
			job->Translate = msdfgen::Vector2(padding / scale, padding / scale);
			AtlasIcon& icon = atlas.Icons[i];
			icon.Valid = true;
			const float margin = float(padding / desc.IconSize);
			const float extent = float(job->Width / double(desc.IconSize));
			icon.PlaneMin = glm::vec2(-margin);
			icon.PlaneMax = glm::vec2(extent - margin);
			job->UvMin = &icon.UvMin;
			job->UvMax = &icon.UvMax;
			jobs.push_back(std::move(job));
		}

		std::vector<Job*> order;
		order.reserve(jobs.size());
		for (auto& job : jobs)
			order.push_back(job.get());
		PlaceJobs(order, atlas.Width, atlas.Height);
		atlas.Pixels.assign(size_t(atlas.Width) * atlas.Height * 4, 0);

		std::atomic<size_t> cursor = 0;
		std::atomic<uint32_t> fallbacks = 0;
		const auto worker = [&]
		{
			for (size_t index = cursor++; index < jobs.size(); index = cursor++)
				Generate(*jobs[index], atlas, desc.DistanceRange, fallbacks);
		};
#if _ITERATOR_DEBUG_LEVEL == 2
		const uint32_t threadCount = 1;
#else
		const uint32_t threadCount = std::clamp(std::thread::hardware_concurrency(), 1u, 16u);
#endif
		std::vector<std::thread> threads;
		for (uint32_t i = 1; i < threadCount; i++)
			threads.emplace_back(worker);
		worker();
		for (std::thread& thread : threads)
			thread.join();
		atlas.RasterIcons = fallbacks;

		const glm::vec2 size(float(atlas.Width), float(atlas.Height));
		for (auto& job : jobs)
		{
			*job->UvMin = glm::vec2(float(job->X), float(job->Y)) / size;
			*job->UvMax = glm::vec2(float(job->X + job->Width), float(job->Y + job->Height)) / size;
		}
		return true;
	}
}
