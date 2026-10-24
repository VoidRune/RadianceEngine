import bpy
import os
import sys

sys.path.append(os.path.dirname(os.path.abspath(__file__)))
import assetkit as kit


def main():
    argv = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
    options = dict(zip(argv[::2], argv[1::2]))
    blend = os.path.abspath(options['--blend'])
    bpy.ops.wm.open_mainfile(filepath=blend)
    scene = bpy.context.scene
    scene.render.engine = 'CYCLES'
    scene.cycles.device = 'GPU' if kit.enable_gpu() else 'CPU'
    objects = [obj for obj in bpy.data.collections['Export'].objects if obj.type == 'MESH']
    folder = os.path.abspath(options.get('--out', os.path.dirname(blend)))
    name = options.get('--name', os.path.splitext(os.path.basename(blend))[0])
    kit.bake_and_export(objects, folder, name, ao_distance=float(scene.get('ao_distance', 0.5)))


if __name__ == '__main__':
    main()
