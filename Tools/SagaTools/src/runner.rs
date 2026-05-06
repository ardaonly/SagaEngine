/// @file runner.rs
/// @brief Process spawning - mirrors C++ ProcessRunner.cpp exactly.
///        Uses std::process::Command (eliminates 80-line QuoteArg).

use std::process::{Command, Stdio};
use std::path::Path;

pub struct Runner;

impl Runner {
    pub fn run(executable: &str, args: &[String]) -> (i32, String) {
        let mut cmd = Command::new(executable);
        for arg in args {
            cmd.arg(arg);
        }
        cmd.stdin(Stdio::inherit());
        cmd.stdout(Stdio::inherit());
        cmd.stderr(Stdio::inherit());
        
        match cmd.status() {
            Ok(status) => {
                let exit_code = status.code().unwrap_or(128);
                (exit_code, String::new())
            }
            Err(e) => {
                (-1, format!("failed to spawn {}: {}", executable, e))
            }
        }
    }
    
    pub fn find_python() -> String {
        if let Ok(env_python) = std::env::var("PYTHON") {
            if !env_python.is_empty() {
                return env_python;
            }
        }
        
        #[cfg(windows)]
        {
            return "py".to_string();
        }
        
        #[cfg(not(windows))]
        {
            return "python3".to_string();
        }
    }
    
    pub fn file_exists<P: AsRef<Path>>(path: P) -> bool {
        let path = path.as_ref();
        path.exists() && path.is_file()
    }
}
