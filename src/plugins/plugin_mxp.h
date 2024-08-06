/*
 * plugin.h -- structures and definitions for mxPlay plugins
 *
 * Copyright (c) 2005-2013 Miro Kropacek; miro.kropacek@gmail.com
 *
 * This file is part of the mxPlay project, multiformat audio player for
 * Atari TT/Falcon computers.
 *
 * mxPlay is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * mxPlay is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with mxPlay; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef _PLUGIN_MXP_H_
#define _PLUGIN_MXP_H_

#include <string.h>

#define MXP_ERROR				0
#define MXP_OK					1
#define MXP_UNIMPLEMENTED		2

#define MXP_FLG_INFOLINE		(1<<29)			/* show on infoline, too */
#define MXP_FLG_MOD_PARAM		(1<<30)			/* module parameter */
#define MXP_FLG_PLG_PARAM		(1<<31)			/* plugin parameter */

#define MXP_FLG_USE_DSP			(1<<0)			/* plugin uses DSP */
#define MXP_FLG_USE_DMA			(1<<1)			/* plugin uses DMA sound system */
#define MXP_FLG_FAST_CPU		(1<<2)			/* plugin needs a fast CPU */
#define MXP_FLG_XBIOS			(1<<3)			/* plugin uses XBIOS calls only (no direct hw access) */
#define MXP_FLG_DONT_LOAD_MODULE	(1<<4)			/* plugins loads modules by itself */
#define MXP_FLG_USER_CODE 		(1<<5)			/* plugin handles supervisor calls by itself */
#define MXP_FLG_ONLY_030		(1<<6)			/* plugin doesn't work correctly on CT60 */

#define MXP_PAR_TYPE_BOOL		0				/* bool value (on/off) */
#define MXP_PAR_TYPE_CHAR		1				/* character field as string */
#define MXP_PAR_TYPE_INT		2				/* integer as string */

struct SInfo
{
	char*	pPluginAuthor;
	char*	pPluginVersion;
	char*	pReplayName;
	char*	pReplayAuthor;
	char*	pReplayVersion;
	long	flags;
};

struct SParameter
{
	char*	pName;
	long	type;
	int (*Set)( void );
	int (*Get)( void );
};

struct SExtension
{
	char*	ext;
	char*	name;
};

struct SModuleParameter
{
	char*	p;	// path or buffer
	size_t	size;
};
union UParameterBuffer
{
	struct SModuleParameter* pModule;
	long	value;
};

struct SAudioPlugin
{
	char				header[4];
	union UParameterBuffer inBuffer;
	int (*RegisterModule)( void );
	int (*PlayTime)( void );
	int (*Songs)( void );
	int (*Init)( void );
	int (*Set)( void );
	int (*Feed)( void );
	int (*Unset)( void );
	int (*UnregisterModule)( void );
	int (*Pause)( void );
	int (*Mute)( void );
	struct SInfo*		pSInfo;
	struct SExtension*	pSExtension;
	struct SParameter*	pSParameter;
};

#endif //_PLUGIN_MXP_H_

