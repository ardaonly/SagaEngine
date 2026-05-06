/// @file registry.rs
/// @brief Two-section registry parser - mirrors C++ Registry.cpp exactly.
///        Uses serde_json for proper \uXXXX handling (fixes bug #1).

use serde::{Deserialize, Serialize};
use std::collections::HashMap;
use std::fs;
use std::path::{Path, PathBuf};

#[derive(Debug, Clone, Deserialize, Serialize)]
pub struct Registry {
    #[serde(default)]
    pub tools: HashMap<String, String>,
    #[serde(default)]
    pub installers: HashMap<String, String>,
    #[serde(skip)]
    pub base_dir: PathBuf,
}

impl Registry {
    /// Load registry from file.
    /// Mirrors Registry::LoadFromFile() from C++ Registry.cpp.
    pub fn load_from_file<P: AsRef<Path>>(path: P) -> Result<Self, String> {
        let path = path.as_ref();
        let raw = fs::read_to_string(path)
            .map_err(|e| format!("cannot open registry file: {}", e))?;

        let contents: String = raw
            .lines()
            .filter(|l| !l.trim_start().starts_with("//"))
            .collect::<Vec<_>>()
            .join("\n");

        let mut registry: Registry = serde_json::from_str(&contents)
            .map_err(|e| format!("failed to load registry: {}", e))?;
        
        registry.base_dir = path.parent()
            .unwrap_or_else(|| Path::new("."))
            .to_path_buf();
        
        Ok(registry)
    }
    
    /// Locate registry file using discovery order.
    /// Mirrors Registry::LocateFile() from C++ Registry.cpp.
    pub fn locate_file(explicit_path: &str, executable_dir: &Path) -> Option<PathBuf> {
        if !explicit_path.is_empty() && Path::new(explicit_path).exists() {
            return Some(PathBuf::from(explicit_path));
        }
        
        if let Ok(env_path) = std::env::var("SAGATOOLS_REGISTRY") {
            if Path::new(&env_path).exists() {
                return Some(PathBuf::from(env_path));
            }
        }
        
        let sibling = executable_dir.join("config/tools.registry.json");
        if sibling.exists() {
            return Some(sibling);
        }
        
        let cwd = PathBuf::from("tools.registry.json");
        if cwd.exists() {
            return Some(cwd);
        }
        
        None
    }
    
    pub fn find_tool_raw(&self, name: &str) -> Option<&String> {
        self.tools.get(name)
    }
    
    pub fn find_installer_raw(&self, name: &str) -> Option<&String> {
        self.installers.get(name)
    }
    
    pub fn base_dir(&self) -> &Path {
        &self.base_dir
    }
}
