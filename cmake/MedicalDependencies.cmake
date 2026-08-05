include_guard(GLOBAL)

set(_ct_backend_default ON)
if(MINGW)
    # 当前医学 SDK 是 MSVC ABI，MinGW 默认只编译 UI 兼容层。
    set(_ct_backend_default OFF)
endif()

option(CT_ENABLE_MEDICAL_BACKEND
    "Enable DICOM, ITK segmentation, and VTK rendering" ${_ct_backend_default})
option(CT_ENABLE_RTK_BACKEND
    "Link RTK reconstruction and its CUDA dependencies" OFF)

macro(ct_configure_medical_dependencies)
    if(CT_ENABLE_RTK_BACKEND AND NOT CT_ENABLE_MEDICAL_BACKEND)
        message(FATAL_ERROR "CT_ENABLE_RTK_BACKEND requires CT_ENABLE_MEDICAL_BACKEND=ON.")
    endif()

    if(CT_ENABLE_MEDICAL_BACKEND)
        if(NOT MSVC)
            message(FATAL_ERROR
                "The installed VTK/ITK/RTK packages use the MSVC ABI. "
                "Use an MSVC kit or disable CT_ENABLE_MEDICAL_BACKEND.")
        endif()

        if(MSVC AND DEFINED MSVC_TOOLSET_VERSION AND
           NOT MSVC_TOOLSET_VERSION STREQUAL "143")
            message(WARNING
                "MSVC v${MSVC_TOOLSET_VERSION} is selected. DICOM/ITK/VTK are "
                "supported, but CUDA 12.8 and RTK GPU reconstruction require v143.")
        endif()

        # 路径优先级：CMake 缓存中的显式 VTK_DIR/ITK_DIR > 同名环境变量 >
        # CT_MEDICAL_SDK_ROOT 派生目录。三方库始终位于仓库外部。
        set(_ct_default_sdk_root "$ENV{CT_MEDICAL_SDK_ROOT}")
        if(NOT _ct_default_sdk_root)
            set(_ct_default_sdk_root "E:/A/GuangSuo")
        endif()
        set(CT_MEDICAL_SDK_ROOT "${_ct_default_sdk_root}" CACHE PATH
            "External root containing VTK_INSTALL and ITK_INSTALL")
        set(CT_VTK_INSTALL_DIR
            "${CT_MEDICAL_SDK_ROOT}/VTK_INSTALL/install_debug" CACHE PATH
            "External VTK installation prefix")
        set(CT_ITK_INSTALL_DIR
            "${CT_MEDICAL_SDK_ROOT}/ITK_INSTALL/install_debug" CACHE PATH
            "External ITK installation prefix; this installation also contains RTK")

        if(NOT VTK_DIR OR VTK_DIR MATCHES "-NOTFOUND$")
            set(_ct_vtk_config_candidates
                "$ENV{VTK_DIR}"
                "${CT_VTK_INSTALL_DIR}/lib/cmake/vtk-9.5"
            )
            foreach(_ct_candidate IN LISTS _ct_vtk_config_candidates)
                if(EXISTS "${_ct_candidate}/vtk-config.cmake" OR
                   EXISTS "${_ct_candidate}/VTKConfig.cmake")
                    set(VTK_DIR "${_ct_candidate}" CACHE PATH
                        "VTK package directory" FORCE)
                    break()
                endif()
            endforeach()
        endif()

        if(NOT ITK_DIR OR ITK_DIR MATCHES "-NOTFOUND$")
            set(_ct_itk_config_candidates
                "$ENV{ITK_DIR}"
                "${CT_ITK_INSTALL_DIR}/lib/cmake/ITK-5.4"
            )
            foreach(_ct_candidate IN LISTS _ct_itk_config_candidates)
                if(EXISTS "${_ct_candidate}/ITKConfig.cmake")
                    set(ITK_DIR "${_ct_candidate}" CACHE PATH
                        "ITK package directory" FORCE)
                    break()
                endif()
            endforeach()
        endif()

        if(NOT EXISTS "${VTK_DIR}/vtk-config.cmake" OR
           NOT EXISTS "${VTK_DIR}/VTK-targets-debug.cmake")
            message(FATAL_ERROR
                "VTK package is incomplete. Set CT_MEDICAL_SDK_ROOT or VTK_DIR.\n"
                "Expected vtk-config.cmake and VTK-targets-debug.cmake in: ${VTK_DIR}")
        endif()
        if(NOT EXISTS "${ITK_DIR}/ITKConfig.cmake" OR
           NOT EXISTS "${ITK_DIR}/ITKTargets-debug.cmake")
            message(FATAL_ERROR
                "ITK package is incomplete. Set CT_MEDICAL_SDK_ROOT or ITK_DIR.\n"
                "Expected ITKConfig.cmake and ITKTargets-debug.cmake in: ${ITK_DIR}")
        endif()

        if(CMAKE_BUILD_TYPE AND NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
            message(FATAL_ERROR
                "The configured VTK/ITK packages contain Debug binaries only. "
                "Choose Debug or supply matching Release packages.")
        endif()

        find_package(VTK 9.5 CONFIG REQUIRED COMPONENTS
            CommonCore
            CommonDataModel
            FiltersCore
            FiltersGeneral
            FiltersGeometry
            ImagingCore
            ImagingColor
            InteractionStyle
            RenderingCore
            RenderingOpenGL2
            RenderingVolumeOpenGL2
            GUISupportQtQuick
        )

        # 本机 ITK 配置会先枚举全部已编译模块，再处理 COMPONENTS。提前标记当前
        # 阶段不用的模块，避免其历史绝对路径污染配置过程。
        set(ITKVtkGlue_LOADED 1)
        set(TotalVariation_LOADED 1)
        set(_ct_itk_components
            ITKCommon
            ITKIOGDCM
            ITKThresholding
            ITKRegionGrowing
        )
        if(CT_ENABLE_RTK_BACKEND)
            find_package(CUDAToolkit 12.8 REQUIRED)
            list(APPEND _ct_itk_components RTK)
        else()
            # RTK 虽已安装，但影像工作站阶段尚未调用重建和 CUDA 内核。
            set(RTK_LOADED 1)
        endif()
        find_package(ITK 5.4 CONFIG REQUIRED COMPONENTS ${_ct_itk_components})
        include(${ITK_USE_FILE})

        message(STATUS "CT third-party dependency summary")
        message(STATUS "  VTK config : ${VTK_DIR}")
        message(STATUS "  ITK config : ${ITK_DIR}")
        if(CT_ENABLE_RTK_BACKEND)
            message(STATUS "  RTK 2.5    : linked (CUDA ${CUDAToolkit_VERSION})")
        else()
            message(STATUS "  RTK 2.5    : installed but not linked (CT_ENABLE_RTK_BACKEND=OFF)")
        endif()
    else()
        message(STATUS "CT medical backend disabled: portable QML/UI mode")
    endif()
endmacro()
