# Jadette - a DirectX 12 3D Engine.

The name Jadette can be interpreted as an acronym for Joel's Awesame DirEctx 
Twelve Three d Engine. It's maybe not exceptionally awesome yet,
but I thought that it could be good with some anticipation for the future. ;)

Anyway, it's mostly a vehicle for my learning of DirectX 12.

## Features

* Wavefront Obj file loader
* Two types of view control - first person free fly and orbit mode
* Picking, moving and rotating objects with the mouse
* Texture mapping
* Roughness, Metalness, AO maps
* Multiple lights
* Colored lights
* Shadow mapping
* Tangent space normal mapping
* Alpha blended transparent objects
* Alpha cutout objects
* Instancing
* Early Z pre-pass
* Vertex colors


## Screenshots

The following screenshot is from https://github.com/j-oel/jadette-treehouse. 
Original [model](https://sketchfab.com/3d-models/tree-house-494788a17a7e4c6d9ea62b43e2730607) by grigoriyarx.

<img src="https://raw.githubusercontent.com/j-oel/jadette-media/master/screenshots/jadette-treehouse1.jpg" 
width="800" alt="Jadette Tree House screenshot 1">


This screenshot is from my standard test scene that is included in this repository 
(very basic models and textures to keep the size of the repo small):

<img src="https://raw.githubusercontent.com/j-oel/jadette-media/master/screenshots/jadette-standard-scene1.jpg" 
width="800" alt="Jadette Standard Scene screenshot 1">


## Demo Video

[![Watch the demo video](https://raw.githubusercontent.com/j-oel/jadette-media/master/other/video.png)](https://vimeo.com/546210373)


## Guide to the code

When I explore a new code base I like to start at the main function and drill my way down. I think this gives a good overview, but in some code bases there might be a lot of layers and what is the most interesting parts might not be immediately obvious, so it sometimes takes some time to find them. However, in the case of Jadette, this should be a pretty good exploration strategy for most people; the main function is located in the file [main.cpp](src/main.cpp). Nevertheless, If you're not so interested in Win32 specifics you could go directly to [Graphics.cpp](src/Graphics.cpp), which is where the high level graphics is controlled from. The functions *update* and *render* are central and good places to start further drill down from.

The file [Scene.cpp](src/Scene.cpp) is also really important and you will end up there pretty soon after Graphics.cpp. The most fundamental low level DirectX functionality, such as initialization, handling of the device and swap chain, is located in [Dx12_display.cpp](src/Dx12_display.cpp). The user interface is in [User_interface.cpp](src/User_interface.cpp) and [View_controller.cpp](src/View_controller.cpp). Low level input is sent from main.cpp to [Input.cpp](src/Input.cpp), which is used by User_interface and View_controller.


## How to build

Build with Visual Studio 2019. If you don't already have it, you can download the free Community version [here](https://visualstudio.microsoft.com/downloads/). When installing, in "Workloads", select "Desktop development with C++" or "Game development with C++", they contain the Windows 10 SDK, which in turn contains DirectX 12, Jadette's only external dependencies. Then open Jadette.sln and select *Build Solution* from the *Build* menu (or hit the keyboard shortcut).


## Usage

To run from inside Visual Studio, select *Start Without Debugging* from the *Debug* menu (or hit the keyboard shortcut, Ctrl+F5 is the default). In Jadette you can press the F1 key to display the help, to learn how the navigation and interaction works.

To enable fullscreen mode you have to edit the file [data/init.cfg](data/init.cfg), find `borderless_windowed_fullscreen` and change the 0 to 1. In that file you can also change the window size for windowed mode, enable invert mouse (there's also a keybord shortcut for this), change the mouse sensitivity, change what scene file should be loaded etc. To load another 3D model or to make your own scene, edit [data/scene.sce](data/scene.sce), or make a completely new scene file (e.g. starting with a copy of the standard scene file, it contains descriptions of the syntax in comments).

To use Jadette in a project of your own, I suggest using git subtree, that way you can easily get changes from this repo. This is how you would do that:

* Create a repository and make at least one commit (or use an existing repo with at least one commit). Then do this:
 
  ```
  git remote add jadette https://github.com/j-oel/jadette.git
  git subtree add --prefix=jadette --squash jadette master
  ```

* Then you can add your own scene file and change init.cfg to use that scene file. And of course add your own code to do something interesting! :)

* When there are changes in the upstream jadette repo, you can get them with:

  ```
  git subtree pull --prefix=jadette --squash jadette master
  ```
