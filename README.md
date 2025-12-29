# Wearable Sensing LSL (dsi2lsl)

---

## Build from Source Guide
This documentation last updated July, 2025.
> [!IMPORTANT]
> This build guide has only been tested using Windows.

This program will only work on Windows. 

This guide provides instructions for locally compiling and running the Wearable Sensing dsi2lsl plugin. Following these steps will allow you to set up the necessary environment and dependencies to build two executable files from the source code:

- ```dsi2lsl.exe:``` A command-line application that runs in the terminal.
- ```dsi2lslGUI.exe:``` The graphical user interface (GUI) application.
### Requirements
Before you can configure the build, you must download the necessary files and install the required software depencdencies on your system.

#### 📥Required Downloads 
Download the following packages and place them in a single parent folder for easy access:

- [App-WearableSensing (LSL Plugin)](https://github.com/labstreaminglayer/App-WearableSensing):
The source code of the plugin itself. Download the repositroy as a zip file and extract it into your designated folder.
- [DSI API](https://wearablesensing.com/files/DSI-API_Current.zip): The current official API from Wearable Sensing
- [LSL Library (v1.16.2)](https://github.com/sccn/liblsl/releases): The latest version of Lab Streaming Layer.

#### 🛠️System Dependencies 
Ensure the following software is installed on your computer.
- [Qt5 Framework](https://www.qt.io/download-dev): The framework used for the application's graphical user interface (GUI). This guide was written using Qt 5.15.2.
- [Visual Studio 2022 Community Edition](https://visualstudio.microsoft.com/downloads/): Provides the C/C++ compiler (MSVC) needed to build the code. During installation, select the "Desktop development with C++" workload.
- [CMake's Official Website](https://cmake.org/download/): The tool used to generate the projecet build files. CMake is included with the Visual Studio workload mentioned above, so this download is not necessary.

### Building the Project
There are two main steps to building the executables: Configuration, where CMake finds your tools and dependencies, and Build, where it compiles the code.

#### Initial File Setup
Before opening the project in VS Code, you must adjust the folder structure.

After following the initial setup open your main project folder (e.g., ```lsl-wearablesensing```) in Visual Studio Code. Your project structure should look like this.

``` 
lsl-wearablesensing
    |--- App-WearableSensing
        |--- .vscode/settings.json
        |--- CLI
        |--- GUI
        |--- DSI-API
        |--- LSL Library
        |--- CMakeLists.txt
```

Edit Project Names/Path: Inside ```CMakeLists.txt```, take a look at lines 24-28. Since the dependencies you download might be earlier or later versions ensure the naming is correct. 

#### CMake Configuration
You need to tell CMake where to find the LSL and Qt5 libraries you downloaded. In VS Code, the easiest way to do this is by creating a workspace settings file.

1. Create a folder named .vscode in the root of your project folder if it doesn't already exist.

2. Inside the .vscode folder, create a new file named settings.json.

3. Copy and paste the following configuration into your settings.json file (Remember that the path to these folders is different from user to user):


```settings.json
{
    "cmake.configureArgs": [
        "-DLSL_DIR=C:\\Users\\Name\\Documents\\lsl-wearablesensing\\LSL Library\\lib\\cmake\\LSL",
        "-DQt5_DIR=C:/Qt5/5.15.2/msvc2019_64/lib/cmake/Qt5"
    ]
}
```

* Note Qt5 can also be named Qt, it depends on what you named the folder during your Qt installation. 

#### Select the Compiler (Kit) 🧰
Now, you'll tell CMake which compiler to use.

1. Open the VS Code Command Palette by pressing ```Ctrl+Shift+P```.

2. Type and select ```>CMake: Select a Kit```.

3. Choose the 64-bit compiler that matches your Visual Studio installation. It will typically be listed as ```Visual Studio Community 2022 release - amd64```.

#### Select the Build Variant ⚙️
Next, set the build type to Release. This is important for creating the final two executables.

1. Open the Command Palette ```(Ctrl+Shift+P)```.

2. Type and select ```>CMake: Select Variant```.

3. Choose ```Release``` from the options.

#### Run the Configuration Step ▶️ (CMake Cofigure)
With the kit and variant selected, you can now generate the project build files.

1. Open the Command Palette ```(Ctrl+Shift+P)```.

2. Type and select ```>CMake: Configure```.

3. If your configuration was successful, the terminal should output something similar to ```Build file shave been written to: build```

#### Build the Project 🚀 (CMake Build)
With the kit and variant selected, you can now generate the project build files.

1. Open the Command Palette ```(Ctrl+Shift+P)```.

2. Type and select ```>CMake: Build```.

3. This will compile the project. Once it has finished you fill find ```dsi2lsl.exe``` and ```dsi2lslGUI.exe``` inside the ```build/Release``` folder.

## Running the GUI Application
The command-line ```dsi2lsl.exe``` can be run directly from the terminal. However, ```dsi2lslGUI.exe``` requires additional files to be present in the same directory before it will open.

The inside of the ```lsl-wearablesensing/build/Release``` should currently look like this.
```
dsi2lsl.exe
dsi2lslGUI.exe
```

#### A Tip for Finding Files
A useful search utility can make this process much easier. This guide used the [Everything search tool](https://www.voidtools.com/) to instantly locate the neccessary files.

#### Gathering the Dependency Files
You need to copy specific files from the Qt, DSI, and LSL folders into your project's ```build\Release``` directory. It's critical to choose the files that match the compiler you used for the build. For this guide, we used the ```64-bit MSVC 2019 compiler```, so all the file paths will contain ```msvc2019_64```.

(```Qt5Core.dll, Qt5Gui.dll, Qt5Widgets.dll, qwindows.dll```) will be located in your Qt installation directory.
```
Qt5\5.15.2\msvc2019_64\bin\Qt5Core.dll
Qt5\5.15.2\msvc2019_64\bin\Qt5Gui.dll
Qt5\5.15.2\msvc2019_64\bin\Qt5Widgets.dll
Qt5\5.15.2\msvc2019_64\plugins\platforms\qwindows.dll
```

(```libDSI-Windows-x86_64.dll```) will be located in the DSI API folder. 
> [!IMPORTANT]
> You will need to rename the file ```libDSI-Windows-x86_64.dll``` to ```libDSI.dll```.

(```lsl.dll```) will be located in the LSL Library folder.
```
DSI_API_v1.20.3_06202025\libDSI.dll
liblsl-1.16.2-Win_amd64\bin\lsl.dll
```
Copy and paste the dependency files from their respective downloaded folders to the ```Release``` folder. 
#### Final Folder Structure
After copying all the files, your ```build/Release``` directory should look as followed:
```
platforms
    |--- qwindows.dll
dsi2lsl.exe
dsi2lslGUI.exe
libDSI.dll
lsl.dll
Qt5Core.dll
Qt5Gui.dll
Qt5Widgets.dll
```
With this setup in place, your now ready to launch the ```dsi2lslGUI.exe```.

## Alternative: Build using bat

Open the ```build-mingw.bat``` and change the paths according to your file structure. 

Run the following code in the terminal

```
.\build-mingw.bat
```

This will create a ```mingwbuild``` folder with the executable and dependencies. 
