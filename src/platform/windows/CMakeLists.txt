add_executable(ZStreamEye WIN32
    windows_launcher.cpp
    ../../../resources/windows/ZStreamEye.rc
)

target_compile_features(ZStreamEye PRIVATE cxx_std_17)

if(MINGW)
    target_link_options(ZStreamEye PRIVATE -municode -static -static-libgcc -static-libstdc++)
endif()
