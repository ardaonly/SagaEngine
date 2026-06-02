# `.sagaproj` Schema v0

`.sagaproj` schema v0 is the current project manifest format used by
SagaProjectKit for validation, resolution, and doctor-style checks.

The schema is early. Treat it as a repo-local tool contract, not a frozen public
SDK contract.

## Example

```json
{
  "schemaVersion": 0,
  "projectId": "multiplayer-sandbox",
  "displayName": "MultiplayerSandbox",
  "engineCompatibility": {
    "minimumVersion": "0.0.9",
    "targetVersion": "0.1-technical-preview",
    "channel": "technical-preview"
  },
  "paths": {
    "diagnostics": "Diagnostics",
    "generatedReports": "Build/Reports"
  },
  "scenes": [],
  "assets": [
    { "id": "main.assets", "path": "Assets", "kind": "directory" }
  ],
  "scriptFolders": [
    { "id": "main.scripts", "path": "Scripts" }
  ],
  "launchProfiles": [
    { "id": "local", "path": "launch_profiles.json" }
  ],
  "packageProfiles": [
    { "id": "package", "path": "package_profiles.json" }
  ]
}
```

The compatibility fields above reflect current fixture data. They are not a
release promise.

## Path Rules

- Paths are project-relative.
- Absolute paths are rejected.
- Parent traversal is rejected.
- Reported relative paths use `/` separators.
- Validation and resolution do not create or rewrite project files.

## Diagnostic Codes

| Code | Meaning |
| --- | --- |
| `Project.Input.Missing` | Project path input does not exist. |
| `Project.Manifest.Missing` | Manifest file is missing. |
| `Project.Manifest.Ambiguous` | Directory contains more than one `.sagaproj`. |
| `Project.Manifest.ParseFailed` | Manifest JSON is invalid. |
| `Project.Manifest.SchemaVersionUnsupported` | Schema version is not `0`. |
| `Project.Manifest.MissingField` | Required manifest field is absent. |
| `Project.Manifest.InvalidField` | Required manifest field has the wrong shape. |
| `Project.Path.AbsoluteNotAllowed` | Project path is absolute. |
| `Project.Path.ParentTraversalNotAllowed` | Project path escapes through `..`. |
| `Project.Reference.Missing` | Referenced file or folder is missing. |
| `Project.Profile.ParseFailed` | Launch or package profile JSON is invalid. |

## Commands

```sh
sagaproject validate --project <path> --out <report>
sagaproject resolve --project <path> --out <report>
sagaproject doctor --project <path> --out <report>
```

Exit codes:

| Code | Meaning |
| --- | --- |
| `0` | Passed. |
| `1` | Validation or doctor check failed. |
| `2` | Invalid command usage. |
| `3` | Missing input path. |
| `4` | Internal tool error. |

## Not Supported

Schema v0 does not provide editor opening, runtime launch, server launch,
package publishing, asset import/cook, project repair, cloud workspaces,
permissions, launch orchestration, or gameplay content validation.
