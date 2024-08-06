#ifdef PLUGIN_MXP

        .global _mx_plugin
		.global	_mx_parameter
		.global	_mx_register_module
		.global	_mx_get_playtime
		.global	_mx_init
		.global	_mx_set
		.global	_mx_feed
		.global	_mx_unset
		.global	_mx_pause
		.global	_mx_mute
		.global	_mx_info
		.global	_mx_extensions
		.global	_mx_settings

_mx_plugin:
        .dc.b   'M','X','P','2'
_mx_parameter:
        ds.l	1
_mx_callbacks:
		dc.l	_mx_register_module
		dc.l	_mx_get_playtime
		dc.l	_mx_get_songs
		dc.l	_mx_init
		dc.l	_mx_set
		dc.l	_mx_feed
		dc.l	_mx_unset
		dc.l	_mx_unregister_module
		dc.l	_mx_pause
		dc.l	_mx_mute
		dc.l	_mx_info
		dc.l	_mx_extensions
		dc.l	_mx_settings

_mx_vars:
        ds.l    1


#endif //PLUGIN_MXP
