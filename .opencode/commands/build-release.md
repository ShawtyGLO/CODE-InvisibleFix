---
description: Compile le projet en Release x64 VST3
agent: build
---

Compile le plugin InvisibleFix en configuration Release x64 en utilisant MSBuild via une Developer Command Prompt for VS 2022 :

```
msbuild Builds\VisualStudio2022\InvisibleFix_VST3.vcxproj /p:Configuration=Release /p:Platform=x64
```

Le VST3 compilé sera dans `Builds/VisualStudio2022/x64/Release/VST3/InvisibleFix.dll`
