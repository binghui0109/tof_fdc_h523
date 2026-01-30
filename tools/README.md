# Instruction
## Download cross compiler
Download arm-none-eabi-gcc cross compiler for your host machine
- Linux: https://developer.arm.com/-/media/Files/downloads/gnu/14.2.rel1/binrel/arm-gnu-toolchain-14.2.rel1-x86_64-arm-none-eabi.tar.xz
- Window: https://developer.arm.com/-/media/Files/downloads/gnu/14.2.rel1/binrel/arm-gnu-toolchain-14.2.rel1-mingw-w64-x86_64-arm-none-eabi.zip

## Add cross compiler to project
extract compiler into `{Projectdir}/tools/gcc` folder
folder structure should looks like this
```
tools
├── gcc
│   ├── 14.2.rel1-x86_64-arm-none-eabi-manifest.txt
│   ├── arm-none-eabi
│   ├── bin
│   ├── include
│   ├── lib
│   ├── libexec
│   ├── license.txt
│   └── share
└── README.md
```
rename compiler folder name into cc. resulting folder structure will be like this 'tools/gcc'