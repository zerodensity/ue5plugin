## READ ME (and be a nice child) IF YOU WANT TO SEE TEXTURE ON MEDIAZ



1. Be sure your graphics card drivers are up to date
2. Download [Vulkan SDK](https://vulkan.lunarg.com/sdk) min version 1.3.204.1
3. Clone [mediaZ](https://github.com/mediaz/mediaz) typelib branch
4. Go to mediaZ root, and with the command line open, type in the following command and press Enter:

    ```./regen.py -c Release -i -p -b```

5. Download [Unreal Engine 5](https://www.unrealengine.com/en-US/download)
6. Go to <em>C:\ProgramData\mediaz\core</em> and create <em>EngineSetting.json</em>  with the content below 

    ```json
    {
    "repo_root": "C:/Dev/mediaz/", // mediaz repo root path
    "plugin_folders": [ "plugins" ],
    "proto_source_tree_mapped_folders": [
        "vcpkg/installed/x64-windows/include",
        "Source/Framework/mzProto",
        "Source/Framework/mzProto/proto_include"
    ],
    "app_registry": [
        {
        "app_name": "UE5",
        "node_name": "UE5",
        "app_key": "UE5",
        "exe_path": "D:\\Games\\UE_5.0\\Engine\\Binaries\\Win64\\UnrealEditor.exe", // UnrealEditor.exe path
        "params": []
        }
    ]
    }

7. Go to <em>path_to_mediaz/mediaz/EngineSettings/Applications</em> then copy UE5 folder and paste into <em>C:\ProgramData\mediaz\core\applications</em> 

8. We will add a system variable named. Follow the steps below: 

    Variable name: __MZ_SDK_DIR__

    Variable value: __path_to_mediaz\mediaz\CMakeBuild\Release\install\SDK__
![](Resources/Screenshot%202022-06-16%20150825.png )

9. We will add 2 new path env variables
    1. __path_to_mediaz\mediaz\CMakeBuild\Release\install\SDKinstalled\x64-windows\bin__
    2. __path_to_mediaz\mediaz\CMakeBuild\Release\install\SDKinstalled\bin__

    ![](Resources/path.png )

10. Create new unreal-5 C++  project 
11. Run mediaz with the command  ```./regen.py -r"mzLauncher mzEditor mzLogger"```
12. Create new Texture Render Target2D and new Remote Control Preset

will be continue..