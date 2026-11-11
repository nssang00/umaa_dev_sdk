# Source / migration inventory (2026-11-11)

## In this repository

- Current v0.8.5 Public API contract and review, including the four-role AutoPilot scenario.
- v0.8.5 illustrative four-role C++11 Component code and earlier v0.8.4 declaration sketch.
- Historical v0.7–v0.8.4 API design and static mapping reviews.
- Handoff context and a resume checklist.

## Not in this public repository

- User-supplied UMAA ICD 6.0 PDF manuals: EO, MO, MM, SA, SEM, SO, expanded EXP, CO-EXP.
- User-supplied expanded IDL ZIP: `umaa-6.0-idl-with-exp-services.zip`, recorded as **594 IDL files** in prior static audit.
- Old v0.7 binary handoff ZIP including an Autopilot mock and reference archives (a prior prototype, not v0.8.4 runtime).
- DDS libraries / RTI dependency binaries, any secret tokens, private machine configuration.

Why: distribution permissions for the supplied standards/reference packages have not been verified. If a later task needs primary source evidence or precise IDL parsing, request the original attachments again rather than reconstituting it from guesses.

## Version lineage

`v0.7` initial component API → `v0.8` Command/Data contract review → `v0.8.1` ICD/IDL static audit → `v0.8.2` source-backed user contract → `v0.8.3` Command/Cancel Operation contract → **`v0.8.4` ServiceKind + Provider/Consumer role binding**.

## Historical audit notes (do not overinterpret)

- Standard six ICD groups: 54 Services, 114 topic operations, matching static IDL topics in former audit.
- With expanded EXP + CO-EXP: 140 Service rows / 139 unique Services; 317 operation rows / 314 unique topic operations, 311/314 exact IDL topic declarations.
- 3 experimental @nested-vs-topic inconsistencies are documented in `docs/history/v0.8.1/UMAA_ICD6_IDL_Coverage_Review_v0.8.1.md`.
- This is **static declaration alignment**, not evidence that public API scenarios compile or runtime meets ICD 6.0.

## Publication precautions

Do not upload full ICD PDFs, copied standards text, binary zip sources, RTI software, local credentials, or generated files of uncertain license into this public repository without permission.

## Additional source documents used in v0.8.5

The user supplied `UMAA_Public_API_Summary_KO.md` and `umaa_starter_kit_hooks.md` as design references. Their contents were selectively reflected in `docs/current/UMAA_Component_SDK_Public_API_v0.8.5.md` and `docs/current/REVIEW_v0.8.5.md`. The original attachments are not assumed to remain mounted in a future session. These are design suggestions, not authoritative generated C++ API specifications.
