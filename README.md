# XafTool
XafTool is a simple extractor for .xaf archives used in certain arcade games.

## Usage
The easiest way is to drag and drop a .xaf archive onto the exe. Doing so will automatically start extracting the xaf
into the folder the archive resides in, which should be the most common use case.
Alternatively, you can run it from the command line with the optional -verbose flag to get information about each file.
```bash 
XafTool.exe -verbose <path_to_xaf>
```

## Building
To build the project you will need cmake and a compiler of your choice, depending on your platform.
On Windows, I recommend Visual Studio 2022. This project uses C++ 23 so as long as your compiler supports it, that's fine.
```bah
mkdir build
cmake ...
```
Should be all you need to get going. 
