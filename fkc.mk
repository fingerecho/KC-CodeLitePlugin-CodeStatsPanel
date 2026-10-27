##
## Auto Generated makefile by CodeLite IDE
## any manual changes will be erased      
##
## Win_x64_Release
ProjectName            :=fkc
ConfigurationName      :=Win_x64_Release
WorkspaceConfiguration :=Release
WorkspacePath          :=D:/Temp/Temp
ProjectPath            :=D:/Temp/fkc
IntermediateDirectory  :=$(ConfigurationName)
OutDir                 := $(IntermediateDirectory)
CurrentFileName        :=
CurrentFilePath        :=
CurrentFileFullPath    :=
User                   :=28328
Date                   :=27/10/2026
CodeLitePath           :=D:/Temp/CodeLite_from_git/codelite/.build-release/install/share/codelite
LinkerName             :=D:/msys64/clang64/bin/clang++.exe
SharedObjectLinkerName :=D:/msys64/clang64/bin/clang++.exe -shared -fPIC
ObjectSuffix           :=.o
DependSuffix           :=
PreprocessSuffix       :=.i
IncludeSwitch          :=-I
LibrarySwitch          :=-l
OutputSwitch           :=-o 
LibraryPathSwitch      :=-L
PreprocessorSwitch     :=-D
SourceSwitch           :=-c 
OutputDirectory        :=$(IntermediateDirectory)
OutputFile             :=$(IntermediateDirectory)/fkc.dll
Preprocessors          :=$(PreprocessorSwitch)__WX__ $(PreprocessorSwitch)WXUSINGDLL $(PreprocessorSwitch)WXUSINGDLL_WXSQLITE3 $(PreprocessorSwitch)WXUSINGDLL_SDK $(PreprocessorSwitch)WXUSINGDLL_CL $(PreprocessorSwitch)YY_NEVER_INTERACTIVE=1 
ObjectSwitch           :=-o 
ArchiveOutputSwitch    := 
PreprocessOnlySwitch   :=-E
ObjectsFileList        :="fkc.txt"
PCHCompileFlags        :=
MakeDirCommand         :=D:/Temp/CodeLite_from_git/codelite/.build-release/install/bin/mkdir.exe -p
RcCmpOptions           := 
RcCompilerName         :=D:/msys64/clang64/bin/windres.exe
LinkOptions            :=  -O2 -LD:/Temp/CodeLite_from_git/codelite/.build-release/wxWidgets-install/lib/clang_x64_dll -pipe -lwxmsw33u -s
IncludePath            :=  $(IncludeSwitch). $(IncludeSwitch). $(IncludeSwitch)..\CodeLite_from_git\codelite\.build-release\install/Interfaces $(IncludeSwitch)..\CodeLite_from_git\codelite\.build-release\install/CodeLite $(IncludeSwitch)..\CodeLite_from_git\codelite\.build-release\install/Plugin $(IncludeSwitch)..\CodeLite_from_git\codelite\.build-release\install/sdk/wxsqlite3/include 
IncludePCH             := 
RcIncludePath          := 
Libs                   := $(LibrarySwitch)plugin $(LibrarySwitch)codelite $(LibrarySwitch)wxsqlite3 
ArLibs                 :=  "libplugin.dll" "libcodelite.dll" "libwxsqlite3.dll" 
LibPath                := $(LibraryPathSwitch). $(LibraryPathSwitch)../lib/gcc_lib $(LibraryPathSwitch)D:/Temp/CodeLite_from_git/codelite/.build-release/lib 

##
## Common variables
## AR, CXX, CC, AS, CXXFLAGS and CFLAGS can be overridden using an environment variable
##
AR       := D:/msys64/clang64/bin/ar.exe -r
CXX      := D:/msys64/clang64/bin/clang++.exe
CC       := D:/msys64/clang64/bin/clang.exe
CXXFLAGS := -std=c++20 -ID:/Temp/CodeLite_from_git/codelite/.build-release/Plugin/wxTerminalCtrl -ID:/Temp/CodeLite_from_git/codelite/submodules/wxTerminalEmulator/src/lib -ID:/Temp/CodeLite_from_git/codelite/submodules/assistant -ID:/Temp/CodeLite_from_git/codelite/Plugin -ID:/Temp/CodeLite_from_git/codelite/CodeLite -ID:/Temp/CodeLite_from_git/codelite/Plugin/aui -ID:/Temp/CodeLite_from_git/codelite/Interfaces -ID:/Temp/CodeLite_from_git/codelite/.build-release/wxWidgets-install/lib/clang_x64_dll/mswu -ID:/Temp/CodeLite_from_git/codelite/.build-release/wxWidgets-install/include -Wno-ignored-attributes -Wno-unknown-pragmas -Wno-unused-private-field -D_FILE_OFFSET_BITS=64 -DWXUSINGDLL -D__WXMSW__ -DHAVE_W32API_H -D_UNICODE -O2 -DwxDEBUG_LEVEL=0 $(Preprocessors)
CFLAGS   :=  $(shell wx-config --cxxflags --debug=no )  -O2 $(Preprocessors)
ASFLAGS  := 
AS       := D:/msys64/clang64/bin/as.exe


##
## User defined environment variables
##
CodeLiteDir:=D:\Program Files\CodeLite/share/codelite
Objects0=$(IntermediateDirectory)/code_stats.cpp$(ObjectSuffix) $(IntermediateDirectory)/code_stats_panel.cpp$(ObjectSuffix) $(IntermediateDirectory)/fkc.cpp$(ObjectSuffix) 



Objects=$(Objects0) 

##
## Main Build Targets 
##
.PHONY: all clean PreBuild PrePreBuild PostBuild MakeIntermediateDirs
all: $(OutputFile)

$(OutputFile): $(IntermediateDirectory)/.d $(Objects) 
	@$(MakeDirCommand) $(@D)
	@echo "" > $(IntermediateDirectory)/.d
	@echo $(Objects0)  > $(ObjectsFileList)
	$(SharedObjectLinkerName) $(OutputSwitch)$(OutputFile) @$(ObjectsFileList) $(LibPath) $(Libs) $(LinkOptions)
	@$(MakeDirCommand) "D:\Temp\Temp/.build-release"
	@echo rebuilt > "D:\Temp\Temp/.build-release/fkc"

PostBuild:
	@echo Executing Post Build commands ...
	cp "Win_x64_Release\fkc.dll" D:/Temp/CodeLite_from_git/codelite/.build-release/install/lib/codelite
	@echo Done

MakeIntermediateDirs:
	@$(MakeDirCommand) "$(ConfigurationName)"


$(IntermediateDirectory)/.d:
	@$(MakeDirCommand) "$(ConfigurationName)"

PreBuild:


##
## Objects
##
$(IntermediateDirectory)/code_stats.cpp$(ObjectSuffix): code_stats.cpp
	$(CXX) $(IncludePCH) $(SourceSwitch) "D:/Temp/fkc/code_stats.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/code_stats.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/code_stats.cpp$(PreprocessSuffix): code_stats.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/code_stats.cpp$(PreprocessSuffix) code_stats.cpp

$(IntermediateDirectory)/code_stats_panel.cpp$(ObjectSuffix): code_stats_panel.cpp
	$(CXX) $(IncludePCH) $(SourceSwitch) "D:/Temp/fkc/code_stats_panel.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/code_stats_panel.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/code_stats_panel.cpp$(PreprocessSuffix): code_stats_panel.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/code_stats_panel.cpp$(PreprocessSuffix) code_stats_panel.cpp

$(IntermediateDirectory)/fkc.cpp$(ObjectSuffix): fkc.cpp
	$(CXX) $(IncludePCH) $(SourceSwitch) "D:/Temp/fkc/fkc.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/fkc.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/fkc.cpp$(PreprocessSuffix): fkc.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/fkc.cpp$(PreprocessSuffix) fkc.cpp

##
## Clean
##
clean:
	$(RM) -r $(ConfigurationName)/


