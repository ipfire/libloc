#
# SYNOPSIS
#
#   AX_PROG_LUA_MODULES([MODULES], [ACTION-IF-TRUE], [ACTION-IF-FALSE])
#
# DESCRIPTION
#
#   Checks to see if the given Lua modules are available. If true the shell
#   commands in ACTION-IF-TRUE are executed. If not the shell commands in
#   ACTION-IF-FALSE are run. Note if $LUA is not set (for example by
#   calling AC_CHECK_PROG, or AC_PATH_PROG), AC_CHECK_PROG(LUA, lua, lua)
#   will be run.
#
#   MODULES is a space separated list of module names. To check for a
#   minimum version of a module, append the version number to the module
#   name, separated by an equals sign.
#
#   Example:
#
#     AX_PROG_LUA_MODULES(module=1.0.3,, AC_MSG_WARN(Need some Lua modules)
#
# LICENSE
#
#   Copyright (c) 2024 Michael Tremer <michael.tremer@lightningwirelabs.com>
#
#   Copying and distribution of this file, with or without modification, are
#   permitted in any medium without royalty provided the copyright notice
#   and this notice are preserved. This file is offered as-is, without any
#   warranty.

AU_ALIAS([AC_PROG_LUA_MODULES], [AX_PROG_LUA_MODULES])
AC_DEFUN([AX_PROG_LUA_MODULES], [dnl
	m4_define([ax_lua_modules])
	m4_foreach([ax_lua_module], m4_split(m4_normalize([$1])), [
		m4_append([ax_lua_modules], [']m4_bpatsubst(ax_lua_module,=,[ ])[' ])
	])

	# Make sure we have Lua
	if test -z "$LUA"; then
		AC_CHECK_PROG(LUA, lua, lua)
	fi

	if test "x$LUA" != x; then
		ax_lua_modules_failed=0
		for ax_lua_module in ax_lua_modules; do
			AC_MSG_CHECKING(for Lua module $ax_lua_module)

			# Would be nice to log result here, but can't rely on autoconf internals
			$LUA -e "require('$ax_lua_module')" > /dev/null 2>&1
			if test $? -ne 0; then
				AC_MSG_RESULT(no);
				ax_lua_modules_failed=1
			else
				AC_MSG_RESULT(ok);
			fi
		done

		# Run optional shell commands
		if test "$ax_lua_modules_failed" = 0; then
			:; $2
		else
			:; $3
		fi
	else
		AC_MSG_WARN(could not find Lua)
	fi
])dnl
