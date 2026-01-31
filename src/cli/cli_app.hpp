/**
 * @file    cli_app.hpp
 * @brief   CLI Application Entry Point
 * @author  AllenK (Kwyshell)
 * @license MIT
 */

#pragma once

namespace gwt::cli {

/**
 * Run the CLI application
 * 
 * @param argc  Argument count
 * @param argv  Argument values
 * @return      Exit code (0 = success)
 */
int run(int argc, char** argv);

/**
 * Run in simple mode (drag & drop / single file)
 * 
 * @param argc  Argument count
 * @param argv  Argument values (file paths only, no flags)
 * @return      Exit code (0 = success)
 */
int run_simple_mode(int argc, char** argv);

/**
 * Check if arguments indicate simple mode
 * Simple mode: one or more file paths without any flags
 */
[[nodiscard]] bool is_simple_mode(int argc, char** argv);

}  // namespace gwt::cli
