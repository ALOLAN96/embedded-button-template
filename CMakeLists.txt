# SPDX-License-Identifier: MIT
# Copyright (c) 2026 ALOLAN
cmake_minimum_required(VERSION 3.16)
project(mod_button_template LANGUAGES C)
add_library(mod_button STATIC
    User/Module/mod_button.c
    User/ThirdParty/MultiButton/multi_button.c)
target_include_directories(mod_button PUBLIC User/Module
    PRIVATE User/ThirdParty/MultiButton)
set_target_properties(mod_button PROPERTIES
    C_STANDARD 99 C_STANDARD_REQUIRED YES C_EXTENSIONS NO)
# 宿主可对 mod_button 目标统一设置 MOD_BUTTON_MAX_COUNT；默认 16。
