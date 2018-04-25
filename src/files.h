#pragma once
#include <string>

/// If you add or remove nodes from the default_config_file, increase the juci
/// version number (JUCI_VERSION) in ../CMakeLists.txt to automatically apply
/// the changes to user's ~/.juci/config/config.json files
const std::string default_config_file = R"RAW({
    "version": ")RAW"+std::string(JUCI_VERSION)+R"RAW(",
    "gtk_theme": {
        "name_comment": "Use \"\" for default theme, At least these two exist on all systems: Adwaita, Raleigh",
        "name": "",
        "variant_comment": "Use \"\" for default variant, and \"dark\" for dark theme variant. Note that not all themes support dark variant, but for instance Adwaita does",
        "variant": ""
    },
    "source": {
        "style_comment": "Use \"\" for default style, and for instance juci-dark or juci-dark-blue together with dark gtk_theme variant. Styles from normal gtksourceview install: classic, cobalt, kate, oblivion, solarized-dark, solarized-light, tango",
        "style": "juci-light",
        "font_comment": "Use \"\" for default font, and for instance \"Monospace 12\" to also set size",)RAW"
#ifdef __APPLE__
R"RAW(
        "font": "Menlo",)RAW"
#else
#ifdef _WIN32
R"RAW(
        "font": "Consolas",)RAW"
#else
R"RAW(
        "font": "Monospace",)RAW"
#endif
#endif
R"RAW(
        "cleanup_whitespace_characters_comment": "Remove trailing whitespace characters on save, and add trailing newline if missing",
        "cleanup_whitespace_characters": false,
        "show_whitespace_characters_comment": "Determines what kind of whitespaces should be drawn. Use comma-separated list of: space, tab, newline, nbsp, leading, text, trailing or all",
        "show_whitespace_characters": "",
        "format_style_on_save_comment": "Performs clang-format on save for C/C++ and other curly-bracket languages supported by clang-format",
        "format_style_on_save": false,
        "format_style_on_save_if_style_file_found_comment": "Format style if format file is found, even if format_style_on_save is false",
        "format_style_on_save_if_style_file_found": false,
        "smart_brackets_comment": "If smart_inserts is enabled, this option is automatically enabled. When inserting an already closed bracket, the cursor might instead be moved, avoiding the need of arrow keys after autocomplete",
        "smart_brackets": true,
        "smart_inserts_comment": "When for instance inserting (, () gets inserted. Applies to: (), [], \", '. Also enables pressing ; inside an expression before a final ) to insert ; at the end of line, and deletions of empty insertions",
        "smart_inserts": true,
        "show_map": true,
        "map_font_size": "1",
        "show_git_diff": true,
        "show_background_pattern": true,
        "show_right_margin": false,
        "right_margin_position": 80,
        "spellcheck_language_comment": "Use \"\" to set language from your locale settings",
        "spellcheck_language": "en_US",
        "auto_tab_char_and_size_comment": "Use false to always use default tab char and size",
        "auto_tab_char_and_size": true,
        "default_tab_char_comment": "Use \"\t\" for regular tab",
        "default_tab_char": " ",
        "default_tab_size": 2,
        "tab_indents_line": true,
        "wrap_lines": false,
        "highlight_current_line": true,
        "show_line_numbers": true,
        "enable_multiple_cursors": false,
        "auto_reload_changed_files": false,
        "clang_format_style_comment": "IndentWidth, AccessModifierOffset and UseTab are set automatically. See http://clang.llvm.org/docs/ClangFormatStyleOptions.html",
        "clang_format_style": "ColumnLimit: 0, MaxEmptyLinesToKeep: 2, SpaceBeforeParens: Never, NamespaceIndentation: All, BreakBeforeBraces: Custom, BraceWrapping: {BeforeElse: true, BeforeCatch: true}",
        "clang_usages_threads_comment": "The number of threads used in finding usages in unparsed files. -1 corresponds to the number of cores available, and 0 disables the search",
        "clang_usages_threads": -1
    },
    "terminal": {
        "history_size": 1000,
        "font_comment": "Use \"\" to use source.font with slightly smaller size",
        "font": ""
    },
    "keybindings": {
        "preferences": "<primary>comma",
        "quit": "<primary>q",
        "file_new_file": "<primary>n",
        "file_new_folder": "<primary><shift>n",
        "file_open_file": "<primary>o",
        "file_open_folder": "<primary><shift>o",
        "file_reload_file": "",
        "file_save": "<primary>s",
        "file_save_as": "<primary><shift>s",
        "file_print": "",
        "edit_undo": "<primary>z",
        "edit_redo": "<primary><shift>z",
        "edit_cut": "<primary>x",
        "edit_copy": "<primary>c",
        "edit_paste": "<primary>v",
        "edit_find": "<primary>f",
        "source_spellcheck": "",
        "source_spellcheck_clear": "",
        "source_spellcheck_next_error": "<primary><shift>e",
        "source_git_next_diff": "<primary>k",
        "source_git_show_diff": "<alt>k",
        "source_indentation_set_buffer_tab": "",
        "source_indentation_auto_indent_buffer": "<primary><shift>i",
        "source_goto_line": "<primary>g",
        "source_center_cursor": "<primary>l",
        "source_cursor_history_back": "<alt>Left",
        "source_cursor_history_forward": "<alt>Right",
        "source_show_completion_comment" : "Add completion keybinding to disable interactive autocompletion",
        "source_show_completion" : "",
        "source_find_file": "<primary>p",
        "source_find_symbol": "<primary><shift>f",
        "source_comments_toggle": "<primary>slash",
        "source_comments_add_documentation": "<primary><alt>slash",
        "source_find_documentation": "<primary><shift>d",
        "source_goto_declaration": "<primary>d",
        "source_goto_type_declaration": "<alt><shift>d",
        "source_goto_implementation": "<primary>i",
        "source_goto_usage": "<primary>u",
        "source_goto_method": "<primary>m",
        "source_rename": "<primary>r",
        "source_implement_method": "<primary><shift>m",
        "source_goto_next_diagnostic": "<primary>e",
        "source_apply_fix_its": "<control>space",
        "project_set_run_arguments": "",
        "project_compile_and_run": "<primary>Return",
        "project_compile": "<primary><shift>Return",
        "project_run_command": "<alt>Return",
        "project_kill_last_running": "<primary>Escape",
        "project_force_kill_last_running": "<primary><shift>Escape",
        "debug_set_run_arguments": "",
        "debug_start_continue": "<primary>y",
        "debug_stop": "<primary><shift>y",
        "debug_kill": "<primary><shift>k",
        "debug_step_over": "<primary>j",
        "debug_step_into": "<primary>t",
        "debug_step_out": "<primary><shift>t",
        "debug_backtrace": "<primary><shift>j",
        "debug_show_variables": "<primary><shift>b",
        "debug_run_command": "<alt><shift>Return",
        "debug_toggle_breakpoint": "<primary>b",
        "debug_goto_stop": "<primary><shift>l",)RAW"
#ifdef __linux
R"RAW(
        "window_next_tab": "<primary>Tab",
        "window_previous_tab": "<primary><shift>Tab",)RAW"
#else
R"RAW(
        "window_next_tab": "<primary><alt>Right",
        "window_previous_tab": "<primary><alt>Left",)RAW"
#endif
R"RAW(
        "window_close_tab": "<primary>w",
        "window_toggle_split": "",)RAW"
#ifdef __APPLE__
R"RAW(
        "window_toggle_full_screen": "<primary><control>f",)RAW"
#else
R"RAW(
        "window_toggle_full_screen": "F11",)RAW"
#endif
R"RAW(
        "window_toggle_tabs": "",
        "window_clear_terminal": ""
    },
    "project": {
        "default_build_path_comment": "Use <project_directory_name> to insert the project top level directory name",
        "default_build_path": "./build",
        "debug_build_path_comment": "Use <project_directory_name> to insert the project top level directory name, and <default_build_path> to insert your default_build_path setting.",
        "debug_build_path": "<default_build_path>/debug",
        "cmake": {)RAW"
#ifdef _WIN32
R"RAW(
            "command": "cmake -G\"MSYS Makefiles\"",)RAW"
#else
R"RAW(
            "command": "cmake",)RAW"
#endif
R"RAW(
            "compile_command": "cmake --build ."
        },
        "meson": {
            "command": "meson",
            "compile_command": "ninja"
        },
        "save_on_compile_or_run": true,
        "clear_terminal_on_compile": true,
        "ctags_command": "ctags",
        "python_command": "PYTHONUNBUFFERED=1 python"
    },
    "documentation_searches": {
        "clang": {
            "separator": "::",
            "queries": {
                "@empty": "https://www.google.com/search?btnI&q=c%2B%2B+",
                "std": "https://www.google.com/search?btnI&q=site:http://www.cplusplus.com/reference/+",
                "boost": "https://www.google.com/search?btnI&q=site:http://www.boost.org/doc/libs/1_59_0/+",
                "Gtk": "https://www.google.com/search?btnI&q=site:https://developer.gnome.org/gtkmm/stable/+",
                "@any": "https://www.google.com/search?btnI&q="
            }
        }
    }
}
)RAW";

const std::string juci_light_style = R"RAW(<?xml version="1.0" encoding="UTF-8"?>

<style-scheme id="juci-light" _name="juci" version="1.0">
  <author>juCi++ team</author>
  <_description>Default juCi++ style</_description>

  <!-- Palette -->
  <color name="white"                       value="#FFFFFF"/>
  <color name="black"                       value="#000000"/>
  <color name="gray"                        value="#888888"/>
  <color name="red"                         value="#CC0000"/>
  <color name="green"                       value="#008800"/>
  <color name="blue"                        value="#0000FF"/>
  <color name="dark-blue"                   value="#002299"/>
  <color name="yellow"                      value="#FFFF00"/>
  <color name="light-yellow"                value="#FFFF88"/>
  <color name="orange"                      value="#FF8800"/>
  <color name="purple"                      value="#990099"/>

  <style name="text"                        foreground="#000000" background="#FFFFFF"/>
  <style name="background-pattern"          background="#rgba(0,0,0,.03)"/>
  <style name="selection"                   background="#4A90D9"/>

  <!-- Current Line Highlighting -->
  <style name="current-line"                background="#rgba(0,0,0,.07)"/>

  <!-- Bracket Matching -->
  <style name="bracket-match"               foreground="white" background="gray" bold="true"/>
  <style name="bracket-mismatch"            foreground="white" background="#FF0000" bold="true"/>

  <!-- Search Matching -->
  <style name="search-match"                background="yellow"/>

  <!-- Language specifics -->
  <style name="def:builtin"                 foreground="blue"/>
  <style name="def:constant"                foreground="blue"/>
  <style name="def:boolean"                 foreground="red"/>
  <style name="def:decimal"                 foreground="red"/>
  <style name="def:base-n-integer"          foreground="red"/>
  <style name="def:floating-point"          foreground="red"/>
  <style name="def:complex"                 foreground="red"/>
  <style name="def:character"               foreground="red"/>
  <style name="def:special-char"            foreground="red"/>

  <!-- Language specifics used by clang-parser in default config -->
  <style name="def:string"                  foreground="red"/>
  <style name="def:comment"                 foreground="gray"/>
  <style name="def:statement"               foreground="blue"/>
  <style name="def:type"                    foreground="blue"/>
  <style name="def:function"                foreground="dark-blue"/>
  <style name="def:identifier"              foreground="purple"/>
  <style name="def:preprocessor"            foreground="green"/>
  <style name="def:error"                   foreground="red"/>
  <style name="def:warning"                 foreground="orange"/>
  <style name="def:note"                    foreground="black" background="light-yellow"/>

  <style name="diff:added-line"             foreground="green"/>
  <style name="diff:removed-line"           foreground="red"/>
  <style name="diff:changed-line"           foreground="orange"/>
  <style name="diff:diff-file"              use-style="def:type"/>
  <style name="diff:location"               use-style="def:statement"/>
  <style name="diff:special-case"           use-style="def:constant"/>

</style-scheme>
)RAW";

const std::string juci_dark_style = R"RAW(<?xml version="1.0" encoding="UTF-8"?>

<style-scheme id="juci-dark" _name="juci" version="1.0">
  <author>juCi++ team</author>
  <_description>Dark juCi++ style</_description>

  <!-- Palette -->
  <color name="white"                       value="#D6D6D6"/>
  <color name="black"                       value="#202428"/>
  <color name="gray"                        value="#919191"/>
  <color name="red"                         value="#FF9999"/>
  <color name="yellow"                      value="#EEEE66"/>
  <color name="green"                       value="#AACC99"/>
  <color name="blue"                        value="#88AAFF"/>
  <color name="light-blue"                  value="#AABBEE"/>
  <color name="purple"                      value="#DD99DD"/>

  <style name="text"                        foreground="white" background="black"/>
  <style name="background-pattern"          background="#rgba(255,255,255,.04)"/>
  <style name="selection"                   background="#215D9C"/>

  <!-- Current Line Highlighting -->
  <style name="current-line"                background="#rgba(255,255,255,.05)"/>

  <!-- Bracket Matching -->
  <style name="bracket-match"               foreground="black" background="gray" bold="true"/>
  <style name="bracket-mismatch"            foreground="black" background="#FF0000" bold="true"/>

  <!-- Search Matching -->
  <style name="search-match"                foreground="black" background="white"/>

  <!-- Language specifics -->
  <style name="def:builtin"                 foreground="blue"/>
  <style name="def:constant"                foreground="blue"/>
  <style name="def:boolean"                 foreground="red"/>
  <style name="def:decimal"                 foreground="red"/>
  <style name="def:base-n-integer"          foreground="red"/>
  <style name="def:floating-point"          foreground="red"/>
  <style name="def:complex"                 foreground="red"/>
  <style name="def:character"               foreground="red"/>
  <style name="def:special-char"            foreground="red"/>

  <!-- Language specifics used by clang-parser in default config -->
  <style name="def:string"                  foreground="red"/>
  <style name="def:comment"                 foreground="gray"/>
  <style name="def:statement"               foreground="blue"/>
  <style name="def:type"                    foreground="blue"/>
  <style name="def:function"                foreground="light-blue"/>
  <style name="def:identifier"              foreground="purple"/>
  <style name="def:preprocessor"            foreground="green"/>
  <style name="def:error"                   foreground="red"/>
  <style name="def:warning"                 foreground="yellow"/>
  <style name="def:note"                    foreground="#E6E6E6" background="#383F46"/>

  <style name="diff:added-line"             foreground="green"/>
  <style name="diff:removed-line"           foreground="red"/>
  <style name="diff:changed-line"           foreground="yellow"/>
  <style name="diff:diff-file"              use-style="def:type"/>
  <style name="diff:location"               use-style="def:statement"/>
  <style name="diff:special-case"           use-style="def:constant"/>

</style-scheme>
)RAW";

const std::string juci_dark_blue_style = R"RAW(<?xml version="1.0" encoding="UTF-8"?>

<style-scheme id="juci-dark-blue" _name="juci" version="1.0">
  <author>juCi++ team</author>
  <_description>Dark blue juCi++ style based on the Emacs deeper blue theme</_description>

  <!-- Palette -->
  <color name="white"                       value="#D6D6D6"/>
  <color name="dark-blue"                   value="#202233"/>
  <color name="gray"                        value="#919191"/>
  <color name="red"                         value="#FF7777"/>
  <color name="yellow"                      value="#FFE100"/>
  <color name="light-yellow"                value="#EAC595"/>
  <color name="blue"                        value="#00CCFF"/>
  <color name="green"                       value="#14ECA8"/>
  <color name="light-blue"                  value="#8BFAFF"/>
  <color name="light-green"                 value="#A0DB6B"/>

  <style name="text"                        foreground="white" background="dark-blue"/>
  <style name="background-pattern"          background="#rgba(255,255,255,.04)"/>
  <style name="selection"                   background="#215D9C"/>

  <!-- Current Line Highlighting -->
  <style name="current-line"                background="#rgba(255,255,255,.05)"/>

  <!-- Bracket Matching -->
  <style name="bracket-match"               foreground="dark-blue" background="gray" bold="true"/>
  <style name="bracket-mismatch"            foreground="dark-blue" background="#FF0000" bold="true"/>

  <!-- Search Matching -->
  <style name="search-match"                foreground="dark-blue" background="white"/>

  <!-- Language specifics -->
  <style name="def:builtin"                 foreground="blue"/>
  <style name="def:constant"                foreground="blue"/>
  <style name="def:boolean"                 foreground="light-yellow"/>
  <style name="def:decimal"                 foreground="light-yellow"/>
  <style name="def:base-n-integer"          foreground="light-yellow"/>
  <style name="def:floating-point"          foreground="light-yellow"/>
  <style name="def:complex"                 foreground="light-yellow"/>
  <style name="def:character"               foreground="light-yellow"/>
  <style name="def:special-char"            foreground="light-yellow"/>

  <!-- Language specifics used by clang-parser in default config -->
  <style name="def:string"                  foreground="light-yellow"/>
  <style name="def:comment"                 foreground="gray"/>
  <style name="def:statement"               foreground="blue"/>
  <style name="def:type"                    foreground="blue"/>
  <style name="def:function"                foreground="light-blue"/>
  <style name="def:identifier"              foreground="light-green"/>
  <style name="def:preprocessor"            foreground="yellow"/>
  <style name="def:error"                   foreground="red"/>
  <style name="def:warning"                 foreground="yellow"/>
  <style name="def:note"                    foreground="#E6E6E6" background="#383C59"/>

  <style name="diff:added-line"             foreground="green"/>
  <style name="diff:removed-line"           foreground="red"/>
  <style name="diff:changed-line"           foreground="yellow"/>
  <style name="diff:diff-file"              use-style="def:type"/>
  <style name="diff:location"               use-style="def:statement"/>
  <style name="diff:special-case"           use-style="def:constant"/>

</style-scheme>
)RAW";
