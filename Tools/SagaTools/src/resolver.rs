/// @file resolver.rs
/// @brief Path resolution - mirrors C++ Resolver.cpp exactly.
///        Uses Rust native PathBuf (fixes bugs #3/#4).

use std::env;
use std::path::{Path, PathBuf};
use crate::registry::Registry;

#[derive(Debug, Clone)]
pub struct Resolution {
    pub available: bool,
    pub resolved_path: String,
    pub source: String,
    pub expanded_value: String,
}

pub struct Resolver;

impl Resolver {
    pub fn resolve(registry: &Registry, name: &str) -> Resolution {
        let mut out = Resolution {
            available: false,
            resolved_path: String::new(),
            source: String::new(),
            expanded_value: String::new(),
        };
        
        let raw = match registry.find_tool_raw(name) {
            Some(r) => r,
            None => return out,
        };
        
        out.expanded_value = Self::expand_env(raw);
        if out.expanded_value.is_empty() {
            return out;
        }
        
        if Self::contains_path_separator(&out.expanded_value) {
            let abs = Self::absolute_from_registry(registry.base_dir(), &out.expanded_value);
            if abs.exists() && abs.is_file() {
                out.available = true;
                out.resolved_path = abs.to_string_lossy().to_string();
                out.source = "registry-path".to_string();
            }
            return out;
        }
        
        if let Some(hit) = Self::search_on_path(&out.expanded_value) {
            out.available = true;
            out.resolved_path = hit.to_string_lossy().to_string();
            out.source = "PATH".to_string();
        }
        
        out
    }
    
    fn expand_env(input: &str) -> String {
        let mut out = String::with_capacity(input.len());
        let bytes = input.as_bytes();
        let mut i = 0;
        
        while i < bytes.len() {
            if bytes[i] == b'$' && i + 1 < bytes.len() && bytes[i + 1] == b'{' {
                i += 2;
                let mut var_name = String::new();
                while i < bytes.len() && bytes[i] != b'}' {
                    var_name.push(bytes[i] as char);
                    i += 1;
                }
                if i < bytes.len() {
                    i += 1;
                }
                
                if let Ok(value) = env::var(&var_name) {
                    out.push_str(&value);
                } else if let Some(value) = env::var_os(&var_name) {
                    if let Some(value_str) = value.to_str() {
                        out.push_str(value_str);
                    }
                }
                continue;
            }
            out.push(bytes[i] as char);
            i += 1;
        }
        
        out
    }
    
    fn contains_path_separator(s: &str) -> bool {
        s.contains('/') || s.contains('\\')
    }
    
    fn absolute_from_registry(base_dir: &Path, path_str: &str) -> PathBuf {
        let p = Path::new(path_str);
        if p.is_absolute() {
            p.to_path_buf()
        } else {
            base_dir.join(p)
        }
    }
    
    fn search_on_path(executable: &str) -> Option<PathBuf> {
        let path_env = env::var("PATH").unwrap_or_default();
        
        #[cfg(windows)]
        let separator = ';';
        #[cfg(not(windows))]
        let separator = ':';
        
        #[cfg(windows)]
        let suffixes = vec![".exe", ".bat", ".cmd", ""];
        #[cfg(not(windows))]
        let suffixes: Vec<&str> = vec![""];
        
        for dir in path_env.split(separator) {
            if dir.is_empty() {
                continue;
            }
            for suffix in &suffixes {
                let candidate = Path::new(dir).join(format!("{}{}", executable, suffix));
                if candidate.exists() && candidate.is_file() {
                    return Some(candidate);
                }
            }
        }
        
        None
    }
}
