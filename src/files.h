#ifndef JUCI_FILES_H_
#define JUCI_FILES_H_
#include <string>

#define JUCI_VERSION "1.2.0-rc3"

const std::string default_config_file = R"RAW({
    "version": ")RAW"+std::string(JUCI_VERSION)+R"RAW(",
    "default_window_size": {
        "width": 800,
        "height": 600
    },
    "gtk_theme": {
        "name_comment": "Use \"\" for default theme, At least these two exist on all systems: Adwaita, Raleigh",
        "name": "",
        "variant_comment": "Use \"\" for default variant, and \"dark\" for dark theme variant. Note that not all themes support dark variant, but for instance Adwaita does",
        "variant": ""
    },
    "terminal": {
        "history_size": 1000,
        "font_comment": "Use \"\" to use source.font with slightly smaller size",
        "font": ""
    },
    "source": {
        "style_comment": "Use \"\" for default style, and for instance juci-dark or juci-dark-blue together with dark gtk_theme variant. Styles from normal gtksourceview install: classic, cobalt, kate, oblivion, solarized-dark, solarized-light, tango",
        "style": "juci-light",
        "font_comment": "Use \"\" for default font, and for instance \"Monospace 12\" to also set size",)RAW"
#ifdef __APPLE__
R"RAW(
        "font": "Menlo 11",)RAW"
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
        "show_map": true,
        "map_font_size": "1",
        "show_git_diff": true,
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
        "clang_types": {
            "8": "def:function",
            "21": "def:function",
            "22": "def:identifier",
            "24": "def:function",
            "25": "def:function",
            "43": "def:type",
            "44": "def:type",
            "45": "def:type",
            "46": "def:identifier",
            "109": "def:string",
            "702": "def:statement",
            "705": "def:comment"
        },
        "clang_format_style_comment": "IndentWidth, AccessModifierOffset and UseTab are set automatically. See http://clang.llvm.org/docs/ClangFormatStyleOptions.html",
        "clang_format_style": "ColumnLimit: 0, MaxEmptyLinesToKeep: 2"
    },
    "keybindings": {
        "preferences": "<primary>comma",
        "quit": "<primary>q",
        "new_file": "<primary>n",
        "new_folder": "<primary><shift>n",
        "open_file": "<primary>o",
        "open_folder": "<primary><shift>o",
        "save": "<primary>s",
        "save_as": "<primary><shift>s",
        "print": "<primary>p",
        "edit_undo": "<primary>z",
        "edit_redo": "<primary><shift>z",
        "edit_cut": "<primary>x",
        "edit_copy": "<primary>c",
        "edit_paste": "<primary>v",
        "edit_find": "<primary>f",
        "edit_set_tab": "",
        "source_spellcheck": "",
        "source_spellcheck_clear": "",
        "source_spellcheck_next_error": "<primary><shift>e",
        "source_git_next_diff": "<primary>k",
        "source_git_show_diff": "",
        "source_indentation_set_buffer_tab": "",
        "source_indentation_auto_indent_buffer": "<primary><shift>i",
        "source_goto_line": "<primary>g",
        "source_center_cursor": "<primary>l",
        "source_find_documentation": "<primary><shift>d",
        "source_goto_declaration": "<primary>d",
        "source_goto_implementation": "<primary>i",
        "source_goto_usage": "<primary>u",
        "source_goto_method": "<primary>m",
        "source_rename": "<primary>r",
        "source_implement_method": "",
        "source_goto_next_diagnostic": "<primary>e",
        "source_apply_fix_its": "<control>space",
        "project_set_run_arguments": "",
        "compile_and_run": "<primary>Return",
        "compile": "<primary><shift>Return",
        "run_command": "<alt>Return",
        "kill_last_running": "<primary>Escape",
        "force_kill_last_running": "<primary><shift>Escape",
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
        "next_tab": "<primary>Tab",
        "previous_tab": "<primary><shift>Tab",)RAW"
#else
R"RAW(
        "next_tab": "<primary><alt>Right",
        "previous_tab": "<primary><alt>Left",)RAW"
#endif
R"RAW(
        "close_tab": "<primary>w",
        "window_toggle_split": ""
    },
    "project": {
        "default_build_path_comment": "Use <project_directory_name> to insert the project top level directory name",
        "default_build_path": "./build",
        "debug_build_path_comment": "Use <project_directory_name> to insert the project top level directory name, and <default_build_path> to insert your default_build_path setting.",
        "debug_build_path": "<default_build_path>/debug",)RAW"
#ifdef _WIN32
R"RAW(
        "cmake_command": "cmake -G\"MSYS Makefiles\"",)RAW"
#else
R"RAW(
        "cmake_command": "cmake",)RAW"
#endif
R"RAW(
        "make_command": "cmake --build .",
        "save_on_compile_or_run": true,
        "clear_terminal_on_compile": true
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
  <color name="white"                       value="#CCCCCC"/>
  <color name="black"                       value="#292929"/>
  <color name="gray"                        value="#888888"/>
  <color name="red"                         value="#FF9999"/>
  <color name="green"                       value="#AACC99"/>
  <color name="blue"                        value="#9090FF"/>
  <color name="light-blue"                  value="#AAAAFF"/>
  <color name="purple"                      value="#DD88DD"/>

  <style name="text"                        foreground="white" background="black"/>
  <style name="background-pattern"          background="#rgba(255,255,255,.04)"/>
  <style name="selection"                   background="#215D9C"/>

  <!-- Current Line Highlighting -->
  <style name="current-line"                background="#rgba(255,255,255,.06)"/>

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
  <style name="def:error"                   foreground="#FF6666"/>
  <style name="def:warning"                 foreground="#FFE100"/>
  <style name="def:note"                    foreground="white" background="#444444"/>

  <style name="diff:added-line"             foreground="green"/>
  <style name="diff:removed-line"           foreground="red"/>
  <style name="diff:changed-line"           foreground="orange"/>
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
  <color name="light-blue"                  value="#00CCFF"/>
  <color name="green"                       value="#14ECA8"/>
  <color name="blue"                        value="#282A40"/>
  <color name="orange"                      value="#FF8800"/>
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
  <style name="def:builtin"                 foreground="light-blue"/>
  <style name="def:constant"                foreground="light-blue"/>
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
  <style name="def:statement"               foreground="light-blue"/>
  <style name="def:type"                    foreground="light-blue"/>
  <style name="def:function"                foreground="green"/>
  <style name="def:identifier"              foreground="light-green"/>
  <style name="def:preprocessor"            foreground="yellow"/>
  <style name="def:error"                   foreground="red"/>
  <style name="def:warning"                 foreground="yellow"/>
  <style name="def:note"                    foreground="white" background="#404466"/>

  <style name="diff:added-line"             foreground="green"/>
  <style name="diff:removed-line"           foreground="red"/>
  <style name="diff:changed-line"           foreground="orange"/>
  <style name="diff:diff-file"              use-style="def:type"/>
  <style name="diff:location"               use-style="def:statement"/>
  <style name="diff:special-case"           use-style="def:constant"/>

</style-scheme>
)RAW";

#endif  // JUCI_FILES_H_
