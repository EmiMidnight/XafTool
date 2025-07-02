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
To build XafTool, I recommend using Visual Studio 2022 or later as it's using C++ 23.
At some point I might replace the WIN32 API calls with something more cross-platform and switch to cmake, but for now it's Windows only.
