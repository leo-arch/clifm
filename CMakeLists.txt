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

set(BIN "clifm")
set(_MANDIR "man/man1")
set(_BASHDIR "share/bash-completion/completions")
set(_ZSHDIR "share/zsh/site-functions")
set(_DESKTOPPREFIX "share/applications")
set(_DESKTOPICONPREFIX "share/icons/hicolor/scalable/apps")
set(_PROG_DATADIR "share/${BIN}")
set(_PLUGINSDIR "${_PROG_DATADIR}/plugins")

install(DIRECTORY DESTINATION bin)
install(DIRECTORY DESTINATION share/man)
install(DIRECTORY DESTINATION "${_MANDIR}")
install(DIRECTORY DESTINATION "${_DESKTOPPREFIX}")
install(DIRECTORY DESTINATION "${_PROG_DATADIR}")
install(DIRECTORY DESTINATION share/icons)
install(DIRECTORY DESTINATION share/icons/hicolor)
install(DIRECTORY DESTINATION share/icons/hicolor/scalable)
install(DIRECTORY DESTINATION "${_DESKTOPICONPREFIX}")
install(DIRECTORY DESTINATION "${_ZSHDIR}")
install(DIRECTORY DESTINATION share/bash-completion)
install(DIRECTORY DESTINATION share/zsh)
install(DIRECTORY DESTINATION "${_ZSHDIR}")
install(DIRECTORY DESTINATION "${_BASHDIR}")

install(TARGETS "${BIN}" DESTINATION bin)

install(DIRECTORY plugins DESTINATION "${_PROG_DATADIR}"
	PATTERN "*.sh"
		PERMISSIONS
			OWNER_READ OWNER_WRITE OWNER_EXECUTE
			GROUP_READ GROUP_EXECUTE
			WORLD_READ WORLD_EXECUTE)

install(DIRECTORY misc/tools DESTINATION "${_PROG_DATADIR}"
	PATTERN "*.py"
		PERMISSIONS
			OWNER_READ OWNER_WRITE OWNER_EXECUTE
			GROUP_READ GROUP_EXECUTE
			WORLD_READ WORLD_EXECUTE)

install(FILES plugins/BFG.cfg plugins/kbgen.c plugins/plugins-helper DESTINATION "${_PLUGINSDIR}")

install(FILES misc/actions.clifm misc/keybindings.clifm misc/mimelist.clifm misc/nets.clifm misc/prompts.clifm misc/readline.clifm misc/clifmrc DESTINATION "${_PROG_DATADIR}")

install(DIRECTORY misc/colors functions DESTINATION "${_PROG_DATADIR}")

install(FILES "misc/${BIN}.desktop" DESTINATION "${_DESKTOPPREFIX}")
install(FILES "misc/logo/${BIN}.svg" DESTINATION "${_DESKTOPICONPREFIX}")

install(FILES misc/manpage DESTINATION "${_MANDIR}" RENAME "${BIN}.1"
	PERMISSIONS
		OWNER_READ OWNER_WRITE
		GROUP_READ
		WORLD_READ)

install(FILES misc/completions.bash DESTINATION "${_BASHDIR}" RENAME "${BIN}")
install(FILES misc/completions.zsh DESTINATION "${_ZSHDIR}" RENAME "_${BIN}")
