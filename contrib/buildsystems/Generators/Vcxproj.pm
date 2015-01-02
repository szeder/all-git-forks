package Generators::Vcxproj;
require Exporter;

use strict;
use vars qw($VERSION);

our $VERSION = '1.00';
our(@ISA, @EXPORT, @EXPORT_OK, @AVAILABLE);
@ISA = qw(Exporter);

BEGIN {
    push @EXPORT_OK, qw(generate);
}

my $guid_index = 0;
my @GUIDS = (
    "{E07B9989-2BF7-4F21-8918-BE22BA467AC3}",
    "{278FFB51-0296-4A44-A81A-22B87B7C3592}",
    "{7346A2C4-F0FD-444F-9EBE-1AF23B2B5650}",
    "{67F421AC-EB34-4D49-820B-3196807B423F}",
    "{385DCFE1-CC8C-4211-A451-80FCFC31CA51}",
    "{97CC46C5-D2CC-4D26-B634-E75792B79916}",
    "{C7CE21FE-6EF8-4012-A5C7-A22BCEDFBA11}",
    "{51575134-3FDF-42D1-BABD-3FB12669C6C9}",
    "{0AE195E4-9823-4B87-8E6F-20C5614AF2FF}",
    "{4B918255-67CA-43BB-A46C-26704B666E6B}",
    "{18CCFEEF-C8EE-4CC1-A265-26F95C9F4649}",
    "{5D5D90FA-01B7-4973-AFE5-CA88C53AC197}",
    "{1F054320-036D-49E1-B384-FB5DF0BC8AC0}",
    "{7CED65EE-F2D9-4171-825B-C7D561FE5786}",
    "{8D341679-0F07-4664-9A56-3BA0DE88B9BC}",
    "{C189FEDC-2957-4BD7-9FA4-7622241EA145}",
    "{66844203-1B9F-4C53-9274-164FFF95B847}",
    "{E4FEA145-DECC-440D-AEEA-598CF381FD43}",
    "{73300A8E-C8AC-41B0-B555-4F596B681BA7}",
    "{873FDEB1-D01D-40BF-A1BF-8BBC58EC0F51}",
    "{7922C8BE-76C5-4AC6-8BF7-885C0F93B782}",
    "{E245D370-308B-4A49-BFC1-1E527827975F}",
    "{F6FA957B-66FC-4ED7-B260-E59BBE4FE813}",
    "{E6055070-0198-431A-BC49-8DB6CEE770AE}",
    "{54159234-C3EB-43DA-906B-CE5DA5C74654}",
    "{594CFC35-0B60-46F6-B8EF-9983ACC1187D}",
    "{D93FCAB7-1F01-48D2-B832-F761B83231A5}",
    "{DBA5E6AC-E7BE-42D3-8703-4E787141526E}",
    "{6171953F-DD26-44C7-A3BE-CC45F86FC11F}",
    "{9E19DDBE-F5E4-4A26-A2FE-0616E04879B8}",
    "{AE81A615-99E3-4885-9CE0-D9CAA193E867}",
    "{FBF4067E-1855-4F6C-8BCD-4D62E801A04D}",
    "{17007948-6593-4AEB-8106-F7884B4F2C19}",
    "{199D4C8D-8639-4DA6-82EF-08668C35DEE0}",
    "{E085E50E-C140-4CF3-BE4B-094B14F0DDD6}",
    "{00785268-A9CC-4E40-AC29-BAC0019159CE}",
    "{4C06F56A-DCDB-46A6-B67C-02339935CF12}",
    "{3A62D3FD-519E-4EC9-8171-D2C1BFEA022F}",
    "{3A62D3FD-519E-4EC9-8171-D2C1BFEA022F}",
    "{9392EB58-D7BA-410B-B1F0-B2FAA6BC89A7}",
    "{2ACAB2D5-E0CE-4027-BCA0-D78B2D7A6C66}",
    "{86E216C3-43CE-481A-BCB2-BE5E62850635}",
    "{FB631291-7923-4B91-9A57-7B18FDBB7A42}",
    "{0A176EC9-E934-45B8-B87F-16C7F4C80039}",
    "{DF55CA80-46E8-4C53-B65B-4990A23DD444}",
    "{3A0F9895-55D2-4710-BE5E-AD7498B5BF44}",
    "{294BDC5A-F448-48B6-8110-DD0A81820F8C}",
    "{4B9F66E9-FAC9-47AB-B1EF-C16756FBFD06}",
    "{72EA49C6-2806-48BD-B81B-D4905102E19C}",
    "{5728EB7E-8929-486C-8CD5-3238D060E768}"
);

sub generate {
    my ($git_dir, $out_dir, $rel_dir, %build_structure) = @_;
    my @libs = @{$build_structure{"LIBS"}};
    foreach (@libs) {
        createLibProject($_, $git_dir, $out_dir, $rel_dir, \%build_structure);
    }

    my @apps = @{$build_structure{"APPS"}};
    foreach (@apps) {
        createAppProject($_, $git_dir, $out_dir, $rel_dir, \%build_structure);
    }

    createGlueProject($git_dir, $out_dir, $rel_dir, %build_structure);
    return 0;
}

sub createLibProject {
    my ($libname, $git_dir, $out_dir, $rel_dir, $build_structure) = @_;
    print "Generate $libname vcxproj lib project\n";
    $rel_dir = "..\\$rel_dir";
    $rel_dir =~ s/\//\\/g;

    my $target = $libname;
    $target =~ s/\//_/g;
    $target =~ s/\.a//;

    my $uuid = $GUIDS[$guid_index];
    $$build_structure{"LIBS_${target}_GUID"} = $uuid;
    $guid_index += 1;

    my @srcs = sort(map("$rel_dir\\$_", @{$$build_structure{"LIBS_${libname}_SOURCES"}}));
    my @sources;
    foreach (@srcs) {
        $_ =~ s/\//\\/g;
        push(@sources, $_);
    }
    my $defines = join(";", sort(@{$$build_structure{"LIBS_${libname}_DEFINES"}}));
    my $includes= join(";", sort(map("\"$rel_dir\\$_\"", @{$$build_structure{"LIBS_${libname}_INCLUDES"}})));
    my $cflags  = join(" ", sort(@{$$build_structure{"LIBS_${libname}_CFLAGS"}}));


    my $cflags_debug = $cflags;
    $cflags_debug =~ s/-MT/-MTd/;
    $cflags_debug =~ s/-O.//;

    my $cflags_release = $cflags;
    $cflags_release =~ s/-MTd/-MT/;

    my @tmp  = @{$$build_structure{"LIBS_${libname}_LFLAGS"}};
    my @tmp2 = ();
    foreach (@tmp) {
        if (/^-LTCG/) {
        } elsif (/^-L/) {
            $_ =~ s/^-L/-LIBPATH:$rel_dir\//;
        }
        push(@tmp2, $_);
    }
    my $lflags = join(" ", sort(@tmp));

    $defines =~ s/-D//g;
    $defines =~ s/\'//g;

    $includes =~ s/-I//g;
	$includes =~ s/\"//g;

    mkdir "$target" || die "Could not create the directory $target for lib project!\n";
    open F, ">$target/$target.vcxproj" || die "Could not open $target/$target.vcxproj for writing!\n";
    binmode F, ":crlf";
    print F << "EOM";
<?xml version="1.0" encoding="utf-8"?>
<Project DefaultTargets="Build" ToolsVersion="12.0" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
  <ItemGroup Label="ProjectConfigurations">
    <ProjectConfiguration Include="Debug|Win32">
      <Configuration>Debug</Configuration>
      <Platform>Win32</Platform>
    </ProjectConfiguration>
    <ProjectConfiguration Include="Release|Win32">
      <Configuration>Release</Configuration>
      <Platform>Win32</Platform>
    </ProjectConfiguration>
  </ItemGroup>
  <PropertyGroup Label="Globals">
    <ProjectGuid>$uuid</ProjectGuid>
    <Keyword>$target</Keyword>
    <RootNamespace>$target</RootNamespace>
  </PropertyGroup>
  <Import Project="\$(VCTargetsPath)\\Microsoft.Cpp.Default.props" />
  <PropertyGroup Condition="'\$(Configuration)|\$(Platform)'=='Debug|Win32'" Label="Configuration">
    <ConfigurationType>StaticLibrary</ConfigurationType>
    <UseDebugLibraries>true</UseDebugLibraries>
    <PlatformToolset>v120</PlatformToolset>
    <CharacterSet>NotSet</CharacterSet>
  </PropertyGroup>
  <PropertyGroup Condition="'\$(Configuration)|\$(Platform)'=='Release|Win32'" Label="Configuration">
    <ConfigurationType>StaticLibrary</ConfigurationType>
    <UseDebugLibraries>false</UseDebugLibraries>
    <PlatformToolset>v120</PlatformToolset>
    <WholeProgramOptimization>true</WholeProgramOptimization>
    <CharacterSet>NotSet</CharacterSet>
  </PropertyGroup>
  <Import Project="\$(VCTargetsPath)\\Microsoft.Cpp.props" />
  <ImportGroup Label="ExtensionSettings">
  </ImportGroup>
  <ImportGroup Label="PropertySheets" Condition="'\$(Configuration)|\$(Platform)'=='Debug|Win32'">
    <Import Project="\$(UserRootDir)\\Microsoft.Cpp.\$(Platform).user.props" Condition="exists('\$(UserRootDir)\\Microsoft.Cpp.\$(Platform).user.props')" Label="LocalAppDataPlatform" />
  </ImportGroup>
  <ImportGroup Label="PropertySheets" Condition="'\$(Configuration)|\$(Platform)'=='Release|Win32'">
    <Import Project="\$(UserRootDir)\\Microsoft.Cpp.\$(Platform).user.props" Condition="exists('\$(UserRootDir)\\Microsoft.Cpp.\$(Platform).user.props')" Label="LocalAppDataPlatform" />
  </ImportGroup>
  <PropertyGroup Label="UserMacros" />
  <PropertyGroup />
  <ItemDefinitionGroup Condition="'\$(Configuration)|\$(Platform)'=='Debug|Win32'">
    <ClCompile>
      <PrecompiledHeader>
      </PrecompiledHeader>
      <WarningLevel>Level3</WarningLevel>
      <AdditionalOptions>$cflags_debug %(AdditionalOptions)</AdditionalOptions>
      <Optimization>Disabled</Optimization>
      <InlineFunctionExpansion>OnlyExplicitInline</InlineFunctionExpansion>
      <AdditionalIncludeDirectories>$includes;%(AdditionalIncludeDirectories)</AdditionalIncludeDirectories>
      <PreprocessorDefinitions>WIN32;_DEBUG;$defines;%(PreprocessorDefinitions)</PreprocessorDefinitions>
      <MinimalRebuild>true</MinimalRebuild>
      <RuntimeLibrary>MultiThreadedDebug</RuntimeLibrary>
      <ProgramDataBaseFileName>\$(IntDir)vc\$(PlatformToolsetVersion).pdb</ProgramDataBaseFileName>
      <DebugInformationFormat>ProgramDatabase</DebugInformationFormat>
    </ClCompile>
    <Lib>
      <SuppressStartupBanner>true</SuppressStartupBanner>
    </Lib>
  </ItemDefinitionGroup>
  <ItemDefinitionGroup Condition="'\$(Configuration)|\$(Platform)'=='Release|Win32'">
    <ClCompile>
      <WarningLevel>Level3</WarningLevel>
      <PrecompiledHeader>
      </PrecompiledHeader>
      <AdditionalOptions>$cflags_release %(AdditionalOptions)</AdditionalOptions>
      <Optimization>MaxSpeed</Optimization>
      <InlineFunctionExpansion>OnlyExplicitInline</InlineFunctionExpansion>
      <IntrinsicFunctions>true</IntrinsicFunctions>
      <AdditionalIncludeDirectories>$includes;%(AdditionalIncludeDirectories)</AdditionalIncludeDirectories>
      <PreprocessorDefinitions>WIN32;NDEBUG;$defines;%(PreprocessorDefinitions)</PreprocessorDefinitions>
      <RuntimeLibrary>MultiThreaded</RuntimeLibrary>
      <FunctionLevelLinking>true</FunctionLevelLinking>
      <ProgramDataBaseFileName>\$(IntDir)\$(TargetName).pdb</ProgramDataBaseFileName>
      <DebugInformationFormat>ProgramDatabase</DebugInformationFormat>
    </ClCompile>
    <Lib>
      <SuppressStartupBanner>true</SuppressStartupBanner>
    </Lib>
  </ItemDefinitionGroup>
  <ItemGroup>
EOM
    foreach(@sources) {
        print F << "EOM";
    <ClCompile Include="$_" />
EOM
    }
    print F << "EOM";
  </ItemGroup>
  <Import Project="\$(VCTargetsPath)\\Microsoft.Cpp.targets" />
  <ImportGroup Label="ExtensionTargets">
  </ImportGroup>
</Project>
EOM
    close F;
}

sub createAppProject {
    my ($appname, $git_dir, $out_dir, $rel_dir, $build_structure) = @_;
    print "Generate $appname vcxproj app project\n";
    $rel_dir = "..\\$rel_dir";
    $rel_dir =~ s/\//\\/g;

    my $target = $appname;
    $target =~ s/\//_/g;
    $target =~ s/\.exe//;

    my $uuid = $GUIDS[$guid_index];
    $$build_structure{"APPS_${target}_GUID"} = $uuid;
    $guid_index += 1;

    my @srcs = sort(map("$rel_dir\\$_", @{$$build_structure{"APPS_${appname}_SOURCES"}}));
    my @sources;
    foreach (@srcs) {
        $_ =~ s/\//\\/g;
        push(@sources, $_);
    }
    my $defines = join(";", sort(@{$$build_structure{"APPS_${appname}_DEFINES"}}));
    my $includes= join(";", sort(map("\"$rel_dir\\$_\"", @{$$build_structure{"APPS_${appname}_INCLUDES"}})));
    my $cflags  = join(" ", sort(@{$$build_structure{"APPS_${appname}_CFLAGS"}}));

    my $cflags_debug = $cflags;
    $cflags_debug =~ s/-MT/-MTd/;
    $cflags_debug =~ s/-O.//;

    my $cflags_release = $cflags;
    $cflags_release =~ s/-MTd/-MT/;

    my $libs;
    foreach (sort(@{$$build_structure{"APPS_${appname}_LIBS"}})) {
        $_ =~ s/\//_/g;
        $libs .= "$_;";
    }

	$libs =~ s/libgit\.lib//g;
    $libs =~ s/xdiff_lib\.lib//g;
	$libs =~ s/vcs-svn_lib\.lib//g;

    my @tmp  = @{$$build_structure{"APPS_${appname}_LFLAGS"}};
    my @tmp2 = ();
    foreach (@tmp) {
        if (/^-LTCG/) {
        } elsif (/^-L/) {
            $_ =~ s/^-L/-LIBPATH:$rel_dir\//;
        }
        push(@tmp2, $_);
    }
    my $lflags = join(" ", sort(@tmp)) . " -LIBPATH:$rel_dir";

    $defines =~ s/-D//g;
    $defines =~ s/\'//g;
    $defines =~ s/\\\\/\\/g;
	$defines =~ s/\\\"/\"/g;

    $includes =~ s/-I//g;
	$includes =~ s/\"//g;

	my $uuid_libgit = $$build_structure{"LIBS_libgit_GUID"};
    my $uuid_xdiff_lib = $$build_structure{"LIBS_xdiff_lib_GUID"};
	my $uuid_vcssvn_lib = $$build_structure{"LIBS_vcs-svn_lib_GUID"};

    mkdir "$target" || die "Could not create the directory $target for lib project!\n";
    open F, ">$target/$target.vcxproj" || die "Could not open $target/$target.vcxproj for writing!\n";
    binmode F, ":crlf";
    print F << "EOM";
<?xml version="1.0" encoding="utf-8"?>
<Project DefaultTargets="Build" ToolsVersion="12.0" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
  <ItemGroup Label="ProjectConfigurations">
    <ProjectConfiguration Include="Debug|Win32">
      <Configuration>Debug</Configuration>
      <Platform>Win32</Platform>
    </ProjectConfiguration>
    <ProjectConfiguration Include="Release|Win32">
      <Configuration>Release</Configuration>
      <Platform>Win32</Platform>
    </ProjectConfiguration>
  </ItemGroup>
  <PropertyGroup Label="Globals">
    <ProjectGuid>$uuid</ProjectGuid>
    <Keyword>$target</Keyword>
    <RootNamespace>$target</RootNamespace>
  </PropertyGroup>
  <Import Project="\$(VCTargetsPath)\\Microsoft.Cpp.Default.props" />
  <PropertyGroup Condition="'\$(Configuration)|\$(Platform)'=='Debug|Win32'" Label="Configuration">
    <ConfigurationType>Application</ConfigurationType>
    <UseDebugLibraries>true</UseDebugLibraries>
    <PlatformToolset>v120</PlatformToolset>
    <CharacterSet>NotSet</CharacterSet>
  </PropertyGroup>
  <PropertyGroup Condition="'\$(Configuration)|\$(Platform)'=='Release|Win32'" Label="Configuration">
    <ConfigurationType>Application</ConfigurationType>
    <UseDebugLibraries>false</UseDebugLibraries>
    <PlatformToolset>v120</PlatformToolset>
    <WholeProgramOptimization>true</WholeProgramOptimization>
    <CharacterSet>NotSet</CharacterSet>
  </PropertyGroup>
  <Import Project="\$(VCTargetsPath)\\Microsoft.Cpp.props" />
  <ImportGroup Label="ExtensionSettings">
  </ImportGroup>
  <ImportGroup Label="PropertySheets" Condition="'\$(Configuration)|\$(Platform)'=='Debug|Win32'">
    <Import Project="\$(UserRootDir)\\Microsoft.Cpp.\$(Platform).user.props" Condition="exists('\$(UserRootDir)\\Microsoft.Cpp.\$(Platform).user.props')" Label="LocalAppDataPlatform" />
  </ImportGroup>
  <ImportGroup Label="PropertySheets" Condition="'\$(Configuration)|\$(Platform)'=='Release|Win32'">
    <Import Project="\$(UserRootDir)\\Microsoft.Cpp.\$(Platform).user.props" Condition="exists('\$(UserRootDir)\\Microsoft.Cpp.\$(Platform).user.props')" Label="LocalAppDataPlatform" />
  </ImportGroup>
  <PropertyGroup Label="UserMacros" />
  <PropertyGroup />
  <ItemDefinitionGroup Condition="'\$(Configuration)|\$(Platform)'=='Debug|Win32'">
    <ClCompile>
      <PrecompiledHeader>
      </PrecompiledHeader>
      <WarningLevel>Level3</WarningLevel>
      <AdditionalOptions>$cflags_debug %(AdditionalOptions)</AdditionalOptions>
      <Optimization>Disabled</Optimization>
      <InlineFunctionExpansion>OnlyExplicitInline</InlineFunctionExpansion>
      <AdditionalIncludeDirectories>$includes;%(AdditionalIncludeDirectories)</AdditionalIncludeDirectories>
      <PreprocessorDefinitions>WIN32;_DEBUG;$defines;%(PreprocessorDefinitions)</PreprocessorDefinitions>
      <MinimalRebuild>true</MinimalRebuild>
      <RuntimeLibrary>MultiThreadedDebug</RuntimeLibrary>
      <ProgramDataBaseFileName>\$(IntDir)vc\$(PlatformToolsetVersion).pdb</ProgramDataBaseFileName>
      <DebugInformationFormat>ProgramDatabase</DebugInformationFormat>
    </ClCompile>
    <Link>
      <AdditionalDependencies>$libs %(AdditionalDependencies)</AdditionalDependencies>
      <AdditionalOptions>$lflags %(AdditionalOptions)</AdditionalOptions>
      <GenerateDebugInformation>true</GenerateDebugInformation>
      <SubSystem>Console</SubSystem>
      <TargetMachine>MachineX86</TargetMachine>
    </Link>
  </ItemDefinitionGroup>
  <ItemDefinitionGroup Condition="'\$(Configuration)|\$(Platform)'=='Release|Win32'">
    <ClCompile>
      <WarningLevel>Level3</WarningLevel>
      <PrecompiledHeader>
      </PrecompiledHeader>
      <AdditionalOptions>$cflags_release %(AdditionalOptions)</AdditionalOptions>
      <Optimization>MaxSpeed</Optimization>
      <InlineFunctionExpansion>OnlyExplicitInline</InlineFunctionExpansion>
      <IntrinsicFunctions>true</IntrinsicFunctions>
      <AdditionalIncludeDirectories>$includes;%(AdditionalIncludeDirectories)</AdditionalIncludeDirectories>
      <PreprocessorDefinitions>WIN32;NDEBUG;$defines;%(PreprocessorDefinitions)</PreprocessorDefinitions>
      <RuntimeLibrary>MultiThreaded</RuntimeLibrary>
      <FunctionLevelLinking>true</FunctionLevelLinking>
      <ProgramDataBaseFileName>\$(IntDir)\$(TargetName).pdb</ProgramDataBaseFileName>
      <DebugInformationFormat>ProgramDatabase</DebugInformationFormat>
    </ClCompile>
    <Link>
      <AdditionalDependencies>$libs %(AdditionalDependencies)</AdditionalDependencies>
      <AdditionalOptions>$lflags %(AdditionalOptions)</AdditionalOptions>
      <GenerateDebugInformation>true</GenerateDebugInformation>
      <SubSystem>Console</SubSystem>
      <TargetMachine>MachineX86</TargetMachine>
      <OptimizeReferences>true</OptimizeReferences>
      <EnableCOMDATFolding>true</EnableCOMDATFolding>
    </Link>
  </ItemDefinitionGroup>
  <ItemGroup>
EOM
    foreach(@sources) {
        print F << "EOM";
    <ClCompile Include="$_" />
EOM
    }
    print F << "EOM";
  </ItemGroup>
  <ItemGroup>
    <ProjectReference Include="..\\libgit\\libgit.vcxproj">
      <Project>$uuid_libgit</Project>
    </ProjectReference>
    <ProjectReference Include="..\\xdiff_lib\\xdiff_lib.vcxproj">
      <Project>$uuid_xdiff_lib</Project>
    </ProjectReference>
	<ProjectReference Include="..\\vcs-svn_lib\\vcs-svn_lib.vcxproj">
      <Project>$uuid_vcssvn_lib</Project>
    </ProjectReference>
  </ItemGroup>
  <Import Project="\$(VCTargetsPath)\\Microsoft.Cpp.targets" />
  <ImportGroup Label="ExtensionTargets">
  </ImportGroup>
</Project>
EOM
    close F;
}

sub createGlueProject {
    my ($git_dir, $out_dir, $rel_dir, %build_structure) = @_;
    print "Generate solutions file\n";
    $rel_dir = "..\\$rel_dir";
    $rel_dir =~ s/\//\\/g;

    my $SLN_PRE  = "Project(\"{8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942}\") = ";
    my $SLN_POST = "\nEndProject\n";

    my @libs = @{$build_structure{"LIBS"}};
    my @tmp;
    foreach (@libs) {
        $_ =~ s/\//_/g;
        $_ =~ s/\.a//;
        push(@tmp, $_);
    }
    @libs = @tmp;

    my @apps = @{$build_structure{"APPS"}};
    @tmp = ();
    foreach (@apps) {
        $_ =~ s/\//_/g;
        $_ =~ s/\.exe//;
        push(@tmp, $_);
    }
    @apps = @tmp;

    open F, ">git.sln" || die "Could not open git.sln for writing!\n";
    binmode F, ":crlf";
    print F "Microsoft Visual Studio Solution File, Format Version 12.00\n";
    print F "# Visual Studio 2013\n";
    print F "VisualStudioVersion = 12.0.30501.0\n";
    print F "MinimumVisualStudioVersion = 10.0.40219.1\n";

    foreach (@libs) {
        my $libname = $_;
        my $uuid = $build_structure{"LIBS_${libname}_GUID"};
        print F "$SLN_PRE";
        print F "\"${libname}\", \"${libname}\\${libname}.vcxproj\", \"${uuid}\"";
        print F "$SLN_POST";
    }
    my $uuid_libgit = $build_structure{"LIBS_libgit_GUID"};
    my $uuid_xdiff_lib = $build_structure{"LIBS_xdiff_lib_GUID"};
    foreach (@apps) {
        my $appname = $_;
        my $uuid = $build_structure{"APPS_${appname}_GUID"};
        print F "$SLN_PRE";
        print F "\"${appname}\", \"${appname}\\${appname}.vcxproj\", \"${uuid}\"\n";
        print F "	ProjectSection(ProjectDependencies) = postProject\n";
        print F "		${uuid_libgit} = ${uuid_libgit}\n";
        print F "		${uuid_xdiff_lib} = ${uuid_xdiff_lib}\n";
        print F "	EndProjectSection";
        print F "$SLN_POST";
    }

    print F << "EOM";
Global
	GlobalSection(SolutionConfigurationPlatforms) = preSolution
		Debug|Win32 = Debug|Win32
		Release|Win32 = Release|Win32
	EndGlobalSection
EOM
    print F << "EOM";
	GlobalSection(ProjectConfigurationPlatforms) = postSolution
EOM
    foreach (@libs) {
        my $libname = $_;
        my $uuid = $build_structure{"LIBS_${libname}_GUID"};
        print F "\t\t${uuid}.Debug|Win32.ActiveCfg = Debug|Win32\n";
        print F "\t\t${uuid}.Debug|Win32.Build.0 = Debug|Win32\n";
        print F "\t\t${uuid}.Release|Win32.ActiveCfg = Release|Win32\n";
        print F "\t\t${uuid}.Release|Win32.Build.0 = Release|Win32\n";
    }
    foreach (@apps) {
        my $appname = $_;
        my $uuid = $build_structure{"APPS_${appname}_GUID"};
        print F "\t\t${uuid}.Debug|Win32.ActiveCfg = Debug|Win32\n";
        print F "\t\t${uuid}.Debug|Win32.Build.0 = Debug|Win32\n";
        print F "\t\t${uuid}.Release|Win32.ActiveCfg = Release|Win32\n";
        print F "\t\t${uuid}.Release|Win32.Build.0 = Release|Win32\n";
    }

    print F << "EOM";
	EndGlobalSection
EndGlobal
EOM
    close F;
}

1;
