"""Build pinned Switch dependencies, the complete engine and its native NRO."""
from pathlib import Path
import argparse
import os
import subprocess
import sys
from audit_compile import ROOT
from fetch_dependencies import main as fetch_dependencies
from prepare_sdl import main as prepare_sdl
from assemble_libraries import main as assemble_libraries
from package_engine import main as package_engine


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--jobs',type=int,default=4)
    parser.add_argument('--reuse-objects',action='store_true',help='reuse only explicitly prepared matching engine objects')
    args=parser.parse_args()
    if args.jobs<1:parser.error('--jobs must be positive')
    if os.name!='nt':raise RuntimeError('This orchestration currently targets the prepared Windows workspace')
    cmake=Path('C:/Strawberry/c/bin/cmake.exe')
    ninja=ROOT/'.kazarov/windows/deps/ninja/ninja.exe'
    fetch_dependencies();prepare_sdl()
    for component,build,target in [('dependencies','switch-dependencies-native','switch_libraries'),('sdl','switch-sdl3-vulkan','SDL3-static')]:
        subprocess.run([str(cmake),'-S',str(ROOT/'port/switch'/component),'-B',str(ROOT/'port/build'/build),'-G','Ninja',
            '-DCMAKE_TOOLCHAIN_FILE='+str(ROOT/'port/switch/toolchain-windows.cmake'),
            '-DCMAKE_MAKE_PROGRAM='+str(ninja),'-DCMAKE_BUILD_TYPE=Release'],cwd=ROOT,check=True)
        subprocess.run([str(cmake),'--build',str(ROOT/'port/build'/build),'--target',target,'-j'+str(args.jobs)],cwd=ROOT,check=True)
    assemble_libraries()
    command=[sys.executable,str(ROOT/'scripts/switch/link_engine.py'),'--jobs',str(args.jobs),
        '--libraries',str(ROOT/'port/build/switch-libraries/libraries.json'),
        '--media-headers',str(ROOT/'.kazarov/switch/deps/ffmpeg-7.1-5/opt/devkitpro/portlibs/switch/include')]
    if args.reuse_objects:command+=['--reuse-objects']
    subprocess.run(command,cwd=ROOT,check=True)
    package_engine()


if __name__=='__main__':main()
