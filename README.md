# 光索科技正交投影 CT 控制台 UI 原型

Qt Widgets interaction prototype for an orthogonal-projection CT imaging workstation. The current release completes the interactive UI framework before integrating imaging engines or device logic:

- four focused pages for patient confirmation, interlock inspection, scan range, and image editing;
- a dominant 2 x 2 workspace for axial, coronal, sagittal, and 3D views;
- a scene tree for projections, volumes, segmentations, and measurements;
- contextual display, reconstruction, segmentation, measurement, and DICOM panels.
- switchable 2 x 2, 1 + 3, 3D-only, and dual-projection layouts;
- DICOM import/PACS/transfer, acquisition progress, reconstruction preview, and safety-lock UI flows.

See [DESIGN_SPEC.md](DESIGN_SPEC.md) for the information architecture, state model, safety constraints, color tokens, Qt mapping, and frontend handoff notes.

All patient, device, dose, image, DICOM, volume, and reconstruction data in this prototype is simulated. It is not clinical software.

## Preview

![光索科技正交投影 CT 控制台](ui-review.png)

Individual page captures: `ui-page-1-patient.png`, `ui-page-2-safety.png`, `ui-page-3-range.png`, and `ui-page-4-editor.png`.

## Build

Qt 5 or Qt 6 with the Widgets module is supported.

```powershell
cmake -S . -B build
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

With a multi-configuration generator on Windows, the executable is generated at:

```text
build/Release/EOS_UI.exe
```
