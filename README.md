# EOS UI prototype

Qt Widgets static prototype containing three switchable pages:

- Patient Info Mode
- Acquisition Mode
- Viewer Mode

The interface intentionally contains no patient, acquisition, device, DICOM, or image-processing business logic.

## Build

Qt 5 or Qt 6 with the Widgets module is supported.

```powershell
cmake -S . -B build
cmake --build build --config Release --parallel
```

With a multi-configuration generator on Windows, the executable is generated at:

```text
build/Release/EOS_UI.exe
```

