cmake_minimum_required(VERSION 3.8)
project(CLIFM)

add_compile_definitions(CLIFM_DATADIR=${CMAKE_INSTALL_PREFIX}/share)

file(GLOB SRC_FILES src/*.c)
file(GLOB HDR_FILES src/*.h)

add_executable(clifm
  ${SRC_FILES}
  ${HDR_FILES}
)

if(APPLE)
  find_package(PkgConfig REQUIRED)
  find_package(Intl REQUIRED)
  pkg_check_modules(Readline REQUIRED readline)
  pkg_check_modules(Magic REQUIRED libmagic)

  target_include_directories(clifm PUBLIC
    ${Intl_INCLUDE_DIRS}
    ${Readline_INCLUDE_DIRS}
    ${Magic_INCLUDE_DIRS})
  target_link_libraries(clifm PUBLIC
    ${Intl_LIBRARIES}
    ${Readline_LINK_LIBRARIES}
    ${Magic_LINK_LIBRARIES}
  )
elseif(UNIX)
  target_link_libraries(clifm PUBLIC
    readline
    acl
    cap
    magic
  )
endif()

install(DIRECTORY DESTINATION bin)
install(DIRECTORY DESTINATION man/man1)
install(DIRECTORY DESTINATION applications)
install(DIRECTORY DESTINATION share/clifm)
install(DIRECTORY DESTINATION share/icons/hicolor/scalable/apps)
install(DIRECTORY DESTINATION share/zsh/site-functions)
install(DIRECTORY DESTINATION share/bash-completion/completions)

install(TARGETS clifm DESTINATION bin)

install(DIRECTORY plugins DESTINATION share/clifm
	PATTERN "*.sh"
		PERMISSIONS
			OWNER_READ OWNER_WRITE OWNER_EXECUTE
			GROUP_READ GROUP_EXECUTE
			WORLD_READ WORLD_EXECUTE)

install(FILES plugins/BFG.cfg plugins/kbgen.c plugins/plugins-helper DESTINATION share/clifm/plugins)

install(FILES misc/actions.cfm misc/keybindings.cfm misc/mimelist.cfm misc/nets.cfm misc/prompts.cfm misc/readline.cfm misc/clifmrc DESTINATION share/clifm)

install (DIRECTORY misc/colors functions DESTINATION share/clifm)

install (FILES misc/clifm.desktop DESTINATION share/applications)
install (FILES misc/logo/clifm.svg DESTINATION share/icons/hicolor/scalable/apps)

install (FILES misc/manpage DESTINATION share/man/man1 RENAME clifm.1
	PERMISSIONS
		OWNER_READ OWNER_WRITE
		GROUP_READ
		WORLD_READ)

install (FILES misc/completions.bash DESTINATION share/bash-completion/completions RENAME clifm)
install (FILES misc/completions.zsh DESTINATION share/zsh/site-functions RENAME _clifm)
