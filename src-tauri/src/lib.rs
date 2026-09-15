use serde::{Deserialize, Serialize};
use std::collections::HashMap;
use std::fs;
use std::path::PathBuf;
use std::sync::Mutex;
use tauri::menu::{MenuBuilder, MenuItemBuilder};
use tauri::tray::{MouseButton, MouseButtonState, TrayIconBuilder, TrayIconEvent};
use tauri::{AppHandle, Emitter, Manager, State, WindowEvent};

const OPCODES_JSON: &str = include_str!("../../opcodes.json");

#[derive(Serialize, Deserialize, Clone)]
#[serde(rename_all = "camelCase")]
struct Settings {
    version: String,
    always_on_top: bool,
    tray_on_close: bool,
}

impl Default for Settings {
    fn default() -> Self {
        Self {
            version: "12.1".to_string(),
            always_on_top: false,
            tray_on_close: true,
        }
    }
}

#[derive(Serialize)]
struct OpcodeEntry {
    value: u32,
    name: String,
}

struct AppState {
    settings: Mutex<Settings>,
    // version -> (value -> name), first name wins on collisions (matches the old app)
    opcodes: HashMap<String, HashMap<u32, String>>,
}

fn settings_path(app: &AppHandle) -> PathBuf {
    app.path()
        .app_local_data_dir()
        .unwrap_or_else(|_| PathBuf::from("."))
        .join("settings.json")
}

fn load_settings_from_disk(app: &AppHandle) -> Settings {
    let path = settings_path(app);
    fs::read_to_string(path)
        .ok()
        .and_then(|json| serde_json::from_str(&json).ok())
        .unwrap_or_default()
}

fn parse_opcodes() -> HashMap<String, HashMap<u32, String>> {
    let raw: HashMap<String, HashMap<String, String>> =
        serde_json::from_str(OPCODES_JSON).unwrap_or_default();

    raw.into_iter()
        .map(|(version, entries)| {
            let mut map = HashMap::new();
            for (key, name) in entries {
                let Some(hex) = key.strip_prefix("0x") else {
                    continue;
                };
                if let Ok(value) = u32::from_str_radix(hex, 16) {
                    map.entry(value).or_insert(name);
                }
            }
            (version, map)
        })
        .collect()
}

#[tauri::command]
fn get_versions(state: State<AppState>) -> Vec<String> {
    let mut versions: Vec<String> = state.opcodes.keys().cloned().collect();
    versions.sort();
    versions
}

#[tauri::command]
fn get_opcodes(state: State<AppState>, version: String) -> Vec<OpcodeEntry> {
    state
        .opcodes
        .get(&version)
        .map(|map| {
            map.iter()
                .map(|(&value, name)| OpcodeEntry {
                    value,
                    name: name.clone(),
                })
                .collect()
        })
        .unwrap_or_default()
}

#[tauri::command]
fn get_settings(state: State<AppState>) -> Settings {
    state.settings.lock().unwrap().clone()
}

#[tauri::command]
fn save_settings(app: AppHandle, state: State<AppState>, settings: Settings) -> Result<(), String> {
    *state.settings.lock().unwrap() = settings.clone();

    let path = settings_path(&app);
    if let Some(dir) = path.parent() {
        fs::create_dir_all(dir).map_err(|e| e.to_string())?;
    }
    fs::write(path, serde_json::to_string(&settings).map_err(|e| e.to_string())?)
        .map_err(|e| e.to_string())
}

fn show_main_window(app: &AppHandle) {
    if let Some(window) = app.get_webview_window("main") {
        let _ = window.show();
        let _ = window.unminimize();
        let _ = window.set_focus();
        let _ = window.emit("window-restored", ());
    }
}

fn show_tray_notification(app: &AppHandle) {
    let app = app.clone();
    std::thread::spawn(move || {
        let Ok(handle) = notify_rust::Notification::new()
            .summary("HexDec TC")
            .body("The application is still running in the system tray.\nClick here to restore.")
            .auto_icon()
            .show()
        else {
            return;
        };

        let _ = handle.wait_for_response(|response: &notify_rust::NotificationResponse| {
            if matches!(response, notify_rust::NotificationResponse::Default) {
                let app2 = app.clone();
                let _ = app.run_on_main_thread(move || show_main_window(&app2));
            }
        });
    });
}

#[cfg_attr(mobile, tauri::mobile_entry_point)]
pub fn run() {
    tauri::Builder::default()
        .plugin(tauri_plugin_single_instance::init(|app, _args, _cwd| {
            show_main_window(app);
        }))
        .plugin(tauri_plugin_clipboard_manager::init())
        .setup(|app| {
            let settings = load_settings_from_disk(&app.handle());

            if settings.always_on_top {
                if let Some(window) = app.get_webview_window("main") {
                    let _ = window.set_always_on_top(true);
                }
            }

            app.manage(AppState {
                settings: Mutex::new(settings),
                opcodes: parse_opcodes(),
            });

            let show = MenuItemBuilder::with_id("show", "Show").build(app)?;
            let quit = MenuItemBuilder::with_id("quit", "Exit").build(app)?;
            let menu = MenuBuilder::new(app).items(&[&show, &quit]).build()?;

            TrayIconBuilder::new()
                .icon(app.default_window_icon().unwrap().clone())
                .tooltip("HexDec TC")
                .menu(&menu)
                .show_menu_on_left_click(false)
                .on_menu_event(|app, event| match event.id().as_ref() {
                    "show" => show_main_window(app),
                    "quit" => app.exit(0),
                    _ => {}
                })
                .on_tray_icon_event(|tray, event| {
                    if let TrayIconEvent::Click {
                        button: MouseButton::Left,
                        button_state: MouseButtonState::Up,
                        ..
                    } = event
                    {
                        show_main_window(tray.app_handle());
                    }
                })
                .build(app)?;

            Ok(())
        })
        .on_window_event(|window, event| {
            if let WindowEvent::CloseRequested { api, .. } = event {
                let tray_on_close = window
                    .try_state::<AppState>()
                    .map(|state| state.settings.lock().unwrap().tray_on_close)
                    .unwrap_or(false);

                if tray_on_close {
                    api.prevent_close();
                    let _ = window.hide();
                    show_tray_notification(window.app_handle());
                }
            }
        })
        .invoke_handler(tauri::generate_handler![
            get_versions,
            get_opcodes,
            get_settings,
            save_settings
        ])
        .run(tauri::generate_context!())
        .expect("error while running tauri application");
}
