/*
 * client.c
 * libpkgconf consumer lifecycle management
 *
 * Copyright (c) 2016 pkgconf authors (see AUTHORS).
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * This software is provided 'as is' and without any warranty, express or
 * implied.  In no event shall the authors be liable for any damages arising
 * from the use of this software.
 */

#include <libpkgconf/libpkgconf.h>

/*
 * !doc
 *
 * libpkgconf `client` module
 * ==========================
 *
 * The libpkgconf `client` module implements the `pkgconf_client_t` "client" object.
 * Client objects store all necessary state for libpkgconf allowing for multiple instances to run
 * in parallel.
 *
 * Client objects are not thread safe, in other words, a client object should not be shared across
 * thread boundaries.
 */

/*
 * !doc
 *
 * .. c:function:: void pkgconf_client_init(pkgconf_client_t *client, pkgconf_error_handler_func_t error_handler)
 *
 *    Initialise a pkgconf client object.
 *
 *    :param pkgconf_client_t* client: The client to initialise.
 *    :param pkgconf_error_handler_func_t error_handler: An optional error handler to use for logging errors.
 *    :param void* error_handler_data: user data passed to optional error handler
 *    :return: nothing
 */
void
pkgconf_client_init(pkgconf_client_t *client, pkgconf_error_handler_func_t error_handler, void *error_handler_data)
{
	client->error_handler_data = error_handler_data;
	client->error_handler = error_handler;
	client->auditf = NULL;

	pkgconf_client_set_sysroot_dir(client, NULL);
	pkgconf_client_set_buildroot_dir(client, NULL);
	pkgconf_client_set_prefix_varname(client, NULL);

	if (client->error_handler == NULL)
		client->error_handler = pkgconf_default_error_handler;

	pkgconf_path_build_from_environ("PKG_CONFIG_SYSTEM_LIBRARY_PATH", SYSTEM_LIBDIR, &client->filter_libdirs, false);
	pkgconf_path_build_from_environ("PKG_CONFIG_SYSTEM_INCLUDE_PATH", SYSTEM_INCLUDEDIR, &client->filter_includedirs, false);

	/* GCC uses these environment variables to define system include paths, so we should check them. */
	pkgconf_path_build_from_environ("LIBRARY_PATH", NULL, &client->filter_libdirs, false);
	pkgconf_path_build_from_environ("CPATH", NULL, &client->filter_includedirs, false);
	pkgconf_path_build_from_environ("C_INCLUDE_PATH", NULL, &client->filter_includedirs, false);
	pkgconf_path_build_from_environ("CPLUS_INCLUDE_PATH", NULL, &client->filter_includedirs, false);
	pkgconf_path_build_from_environ("OBJC_INCLUDE_PATH", NULL, &client->filter_includedirs, false);

#ifdef _WIN32
	/* also use the path lists that MSVC uses on windows */
	pkgconf_path_build_from_environ("INCLUDE", NULL, &client->filter_includedirs, false);
#endif
}

/*
 * !doc
 *
 * .. c:function:: pkgconf_client_t* pkgconf_client_new(pkgconf_error_handler_func_t error_handler)
 *
 *    Allocate and initialise a pkgconf client object.
 *
 *    :param pkgconf_error_handler_func_t error_handler: An optional error handler to use for logging errors.
 *    :param void* error_handler_data: user data passed to optional error handler
 *    :return: A pkgconf client object.
 *    :rtype: pkgconf_client_t*
 */
pkgconf_client_t *
pkgconf_client_new(pkgconf_error_handler_func_t error_handler, void *error_handler_data)
{
	pkgconf_client_t *out = calloc(sizeof(pkgconf_client_t), 1);
	pkgconf_client_init(out, error_handler, error_handler_data);
	return out;
}

/*
 * !doc
 *
 * .. c:function:: void pkgconf_client_deinit(pkgconf_client_t *client)
 *
 *    Release resources belonging to a pkgconf client object.
 *
 *    :param pkgconf_client_t* client: The client to deinitialise.
 *    :return: nothing
 */
void
pkgconf_client_deinit(pkgconf_client_t *client)
{
	if (client->prefix_varname != NULL)
		free(client->prefix_varname);

	if (client->sysroot_dir != NULL)
		free(client->sysroot_dir);

	if (client->buildroot_dir != NULL)
		free(client->buildroot_dir);

	pkgconf_tuple_free_global(client);
	pkgconf_path_free(&client->dir_list);
	pkgconf_cache_free(client);
}

/*
 * !doc
 *
 * .. c:function:: void pkgconf_client_free(pkgconf_client_t *client)
 *
 *    Release resources belonging to a pkgconf client object and then free the client object itself.
 *
 *    :param pkgconf_client_t* client: The client to deinitialise and free.
 *    :return: nothing
 */
void
pkgconf_client_free(pkgconf_client_t *client)
{
	pkgconf_client_deinit(client);
	free(client);
}

/*
 * !doc
 *
 * .. c:function:: const char *pkgconf_client_get_sysroot_dir(const pkgconf_client_t *client)
 *
 *    Retrieves the client's sysroot directory (if any).
 *
 *    :param pkgconf_client_t* client: The client object being accessed.
 *    :return: A string containing the sysroot directory or NULL.
 *    :rtype: const char *
 */
const char *
pkgconf_client_get_sysroot_dir(const pkgconf_client_t *client)
{
	return client->sysroot_dir;
}

/*
 * !doc
 *
 * .. c:function:: void pkgconf_client_set_sysroot_dir(pkgconf_client_t *client, const char *sysroot_dir)
 *
 *    Sets or clears the sysroot directory on a client object.  Any previous sysroot directory setting is
 *    automatically released if one was previously set.
 *
 *    Additionally, the global tuple ``$(pc_sysrootdir)`` is set as appropriate based on the new setting.
 *
 *    :param pkgconf_client_t* client: The client object being modified.
 *    :param char* sysroot_dir: The sysroot directory to set or NULL to unset.
 *    :return: nothing
 */
void
pkgconf_client_set_sysroot_dir(pkgconf_client_t *client, const char *sysroot_dir)
{
	if (client->sysroot_dir != NULL)
		free(client->sysroot_dir);

	client->sysroot_dir = sysroot_dir != NULL ? strdup(sysroot_dir) : NULL;

	pkgconf_tuple_add_global(client, "pc_sysrootdir", client->sysroot_dir != NULL ? client->sysroot_dir : "/");
}

/*
 * !doc
 *
 * .. c:function:: const char *pkgconf_client_get_buildroot_dir(const pkgconf_client_t *client)
 *
 *    Retrieves the client's buildroot directory (if any).
 *
 *    :param pkgconf_client_t* client: The client object being accessed.
 *    :return: A string containing the buildroot directory or NULL.
 *    :rtype: const char *
 */
const char *
pkgconf_client_get_buildroot_dir(const pkgconf_client_t *client)
{
	return client->buildroot_dir;
}

/*
 * !doc
 *
 * .. c:function:: void pkgconf_client_set_buildroot_dir(pkgconf_client_t *client, const char *buildroot_dir)
 *
 *    Sets or clears the buildroot directory on a client object.  Any previous buildroot directory setting is
 *    automatically released if one was previously set.
 *
 *    Additionally, the global tuple ``$(pc_top_builddir)`` is set as appropriate based on the new setting.
 *
 *    :param pkgconf_client_t* client: The client object being modified.
 *    :param char* buildroot_dir: The buildroot directory to set or NULL to unset.
 *    :return: nothing
 */
void
pkgconf_client_set_buildroot_dir(pkgconf_client_t *client, const char *buildroot_dir)
{
	if (client->buildroot_dir != NULL)
		free(client->buildroot_dir);

	client->buildroot_dir = buildroot_dir != NULL ? strdup(buildroot_dir) : NULL;

	pkgconf_tuple_add_global(client, "pc_top_builddir", client->buildroot_dir != NULL ? client->buildroot_dir : "$(top_builddir)");
}

/*
 * !doc
 *
 * .. c:function:: bool pkgconf_error(const pkgconf_client_t *client, const char *format, ...)
 *
 *    Report an error to a client-registered error handler.
 *
 *    :param pkgconf_client_t* client: The pkgconf client object to report the error to.
 *    :param char* format: A printf-style format string to use for formatting the error message.
 *    :return: true if the error handler processed the message, else false.
 *    :rtype: bool
 */
bool
pkgconf_error(const pkgconf_client_t *client, const char *format, ...)
{
	char errbuf[PKGCONF_BUFSIZE];
	va_list va;

	va_start(va, format);
	vsnprintf(errbuf, sizeof errbuf, format, va);
	va_end(va);

	return client->error_handler(errbuf, client, client->error_handler_data);
}

/*
 * !doc
 *
 * .. c:function:: bool pkgconf_default_error_handler(const char *msg, const pkgconf_client_t *client, const void *data)
 *
 *    The default pkgconf error handler.
 *
 *    :param char* msg: The error message to handle.
 *    :param pkgconf_client_t* client: The client object the error originated from.
 *    :param void* data: An opaque pointer to extra data associated with the client for error handling.
 *    :return: true (the function does nothing to process the message)
 *    :rtype: bool
 */
bool
pkgconf_default_error_handler(const char *msg, const pkgconf_client_t *client, const void *data)
{
	(void) msg;
	(void) client;
	(void) data;

	return true;
}

/*
 * !doc
 *
 * .. c:function:: unsigned int pkgconf_client_get_flags(const pkgconf_client_t *client)
 *
 *    Retrieves resolver-specific flags associated with a client object.
 *
 *    :param pkgconf_client_t* client: The client object to retrieve the resolver-specific flags from.
 *    :return: a bitfield of resolver-specific flags
 *    :rtype: uint
 */
unsigned int
pkgconf_client_get_flags(const pkgconf_client_t *client)
{
	return client->flags;
}

/*
 * !doc
 *
 * .. c:function:: void pkgconf_client_set_flags(pkgconf_client_t *client, unsigned int flags)
 *
 *    Sets resolver-specific flags associated with a client object.
 *
 *    :param pkgconf_client_t* client: The client object to set the resolver-specific flags on.
 *    :return: nothing
 */
void
pkgconf_client_set_flags(pkgconf_client_t *client, unsigned int flags)
{
	client->flags = flags;
}

/*
 * !doc
 *
 * .. c:function:: const char *pkgconf_client_get_prefix_varname(const pkgconf_client_t *client)
 *
 *    Retrieves the name of the variable that should contain a module's prefix.
 *    In some cases, it is necessary to override this variable to allow proper path relocation.
 *
 *    :param pkgconf_client_t* client: The client object to retrieve the prefix variable name from.
 *    :return: the prefix variable name as a string
 *    :rtype: const char *
 */
const char *
pkgconf_client_get_prefix_varname(const pkgconf_client_t *client)
{
	return client->prefix_varname;
}

/*
 * !doc
 *
 * .. c:function:: void pkgconf_client_set_prefix_varname(pkgconf_client_t *client, const char *prefix_varname)
 *
 *    Sets the name of the variable that should contain a module's prefix.
 *    If the variable name is ``NULL``, then the default variable name (``prefix``) is used.
 *
 *    :param pkgconf_client_t* client: The client object to set the prefix variable name on.
 *    :param char* prefix_varname: The prefix variable name to set.
 *    :return: nothing
 */
void
pkgconf_client_set_prefix_varname(pkgconf_client_t *client, const char *prefix_varname)
{
	if (prefix_varname == NULL)
		prefix_varname = "prefix";

	if (client->prefix_varname != NULL)
		free(client->prefix_varname);

	client->prefix_varname = strdup(prefix_varname);
}
