#pragma once
#include <algorithm>
#include <cmath>

namespace Rdn
{
	class FramePacer
	{
	public:
		double Pace(double interval, double refreshRate)
		{
			if (refreshRate <= 0.0)
			{
				Reset();
				return interval;
			}
			const double nominal = 1.0 / refreshRate;
			if (std::abs(m_Nominal - nominal) > nominal * 0.001)
			{
				m_Nominal = nominal;
				m_Average = nominal;
				m_Locked = false;
			}
			if (interval < nominal * 3.0)
				m_Average += (interval - m_Average) / 240.0;

			if (std::abs(m_Average - nominal) >= nominal * 0.03)
			{
				m_Locked = false;
				return interval;
			}
			if (!m_Locked)
			{
				m_Locked = true;
				m_Period = m_Average;
				m_Debt = 0.0;
				m_Peak = 0.0;
			}

			m_Debt = std::clamp(m_Debt + interval, -1.5 * m_Period, 0.25);
			const double steps = std::max(1.0, std::round(m_Debt / m_Period));
			const double paced = steps * m_Period;
			m_Debt -= paced;

			m_Peak = std::max(m_Debt, m_Peak + (m_Debt - m_Peak) * 0.05);
			const double correction = m_Peak * 0.02;
			m_Debt -= correction;
			m_Period = std::clamp(m_Period + m_Peak * 0.0005, nominal * 0.97, nominal * 1.03);
			return paced + correction;
		}

		void Reset()
		{
			m_Nominal = 0.0;
			m_Locked = false;
		}

		bool IsLocked() const { return m_Locked; }
		double GetPeriod() const { return m_Period; }

	private:
		double m_Nominal = 0.0;
		double m_Average = 0.0;
		double m_Period = 0.0;
		double m_Debt = 0.0;
		double m_Peak = 0.0;
		bool m_Locked = false;
	};
}
