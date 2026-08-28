# BoatInfo agent guidance

This repository is an OpenCPN plugin and follows the shared development playbook in `jkuhnen/opencpn-plugin-devkit`.

Before changing code:

- Read the current task/issue first and keep the implementation within its scope.
- Read the DevKit `AGENTS.md` plus the relevant files in `docs/`, especially `OPENCPN_DEVELOPMENT.md`, `PLUGIN_ARCHITECTURE.md`, `DESIGN_SYSTEM.md`, `MARITIME_HMI.md`, `BUILD_WINDOWS.md`, `CI.md`, `PACKAGING.md`, and `GIT_WORKFLOW.md`.
- Verify version-sensitive OpenCPN behavior against the target API header and authoritative upstream source; do not invent APIs or callbacks.
- Preserve the current OpenCPN API/build family unless a task explicitly requires a verified migration.
- Keep the OpenCPN adapter thin. Keep data parsing, presentation and styling responsibilities separable as the plugin grows.
- Use OpenCPN global colors and scheme callbacks for host-integrated styling. Use DPI-aware logical dimensions and do not scatter decorative RGB literals through widgets.
- Treat maritime HMI references as design guidance only. Never claim ECDIS, IMO, IEC, IHO, type-approval, or regulatory compliance without the applicable evidence and certification.
- Work on a dedicated branch and hand changes over through a pull request. Do not force-push or rewrite history.
- Do not commit build output, credentials, signing material, private chart data, tokens, or machine-specific paths.
- Report checks performed and explicitly list validation that still requires a real OpenCPN runtime, Signal K source, platform or device.

Current product identity: **BoatInfo**. Avoid vessel-specific branding such as `BenchyNav` and avoid resurrecting legacy `testplugin` identifiers in active plugin code or metadata.
