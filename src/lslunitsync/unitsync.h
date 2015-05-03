#ifndef LIBSPRINGLOBBY_HEADERGUARD_SPRINGUNITSYNC_H
#define LIBSPRINGLOBBY_HEADERGUARD_SPRINGUNITSYNC_H

#include "mmoptionmodel.h"
#include "data.h"
#include "mru_cache.h"
#include <lslutils/type_forwards.h>

#include <boost/thread/mutex.hpp>
#include <boost/signals2/signal.hpp>
#include <map>

#ifdef HAVE_WX
#include <wx/event.h>
#endif
namespace LSL
{

class UnitsyncImage;
struct GameOptions;
struct CachedMapInfo;
struct SpringMapInfo;
class UnitsyncLib;
class WorkerThread;

#ifdef HAVE_WX
extern const wxEventType UnitSyncAsyncOperationCompletedEvt;
#endif

class Unitsync : public boost::noncopyable
{
private:
	typedef boost::signals2::signal<void(std::string)>
	    StringSignalType;

public:
	typedef StringSignalType::slot_type
	    StringSignalSlotType;

	Unitsync();
	~Unitsync();

	StringVector GetModList() const;
	bool ModExists(const std::string& modname, const std::string& hash = "") const;
	UnitsyncMod GetMod(const std::string& modname);
	UnitsyncMod GetMod(int index);

	GameOptions GetModOptions(const std::string& name);
	StringVector GetModDeps(const std::string& name) const;

	StringVector GetMapList() const;
	StringVector GetModValidMapList(const std::string& modname) const;
	bool MapExists(const std::string& mapname, const std::string& hash = "") const;

	UnitsyncMap GetMap(const std::string& mapname);
	UnitsyncMap GetMap(int index);
	GameOptions GetMapOptions(const std::string& name);

	StringVector GetSides(const std::string& modname);
	UnitsyncImage GetSidePicture(const std::string& modname, const std::string& SideName);

	bool LoadUnitSyncLib(const std::string& unitsyncloc);
	void FreeUnitSyncLib();

	bool IsLoaded() const;

	std::string GetSpringVersion() const;
	void UnSetCurrentMod();

	StringVector GetAIList(const std::string& modname) const;
	StringVector GetAIInfos(int index) const;
	GameOptions GetAIOptions(const std::string& modname, int index);


	StringVector GetUnitsList(const std::string& modname);

	/// get minimap rescaled to given width x height
	UnitsyncImage GetMinimap(const std::string& mapname, int width, int height);
	/// get metalmap rescaled to given width x height
	UnitsyncImage GetMetalmap(const std::string& mapname, int width, int height);
	/// get heightmap rescaled to given width x height
	UnitsyncImage GetHeightmap(const std::string& mapname, int width, int height);

	bool ReloadUnitSyncLib();

	void SetSpringDataPath(const std::string& path);
	bool GetSpringDataPath(std::string& path);

	StringVector GetPlaybackList(bool ReplayType = true) const; //savegames otehrwise

	std::string GetArchivePath(const std::string& name) const;

	/// schedule a map for prefetching
	void PrefetchMap(const std::string& mapname);

	boost::signals2::connection RegisterEvtHandler(const StringSignalSlotType& handler);
	void UnregisterEvtHandler(boost::signals2::connection& conn);
	void PostEvent(const std::string& evt); // helper for WorkItems

	void LoadUnitSyncLibAsync(const std::string& filename);

	int GetSpringConfigInt(const std::string& name, int defvalue);
	float GetSpringConfigFloat(const std::string& name, float defvalue);
	std::string GetSpringConfigString(const std::string& name, const std::string& defvalue);

	void SetSpringConfigInt(const std::string& name, int value);
	void SetSpringConfigString(const std::string& name, const std::string& value);
	void SetSpringConfigFloat(const std::string& name, float value);
	std::string GetConfigFilePath();

	void GetMapExAsync(const std::string& mapname);
	void GetMinimapAsync(const std::string& mapname, int width, int height);
	void GetMetalmapAsync(const std::string& mapname, int width, int height);
	void GetHeightmapAsync(const std::string& mapname, int width, int height);

private:
	void ClearCache();
	void GetMinimapAsync(const std::string& mapname);
	void GetMetalmapAsync(const std::string& mapname);
	void GetHeightmapAsync(const std::string& mapname);

	/// get minimap with native width x height
	UnitsyncImage GetMinimap(const std::string& mapname);
	/// get metalmap with native width x height
	UnitsyncImage GetMetalmap(const std::string& mapname);
	/// get heightmap with native width x height
	UnitsyncImage GetHeightmap(const std::string& mapname);

	bool FileExists(const std::string& name) const;
	std::string GetTextfileAsString(const std::string& modname, const std::string& file_path);

	StringVector GetMapDeps(const std::string& name);

	UnitsyncImage GetImage(const std::string& modname, const std::string& image_path, bool useWhiteAsTransparent = true) const;
	typedef std::map<std::pair<std::string, std::string>, std::string> ShortnameVersionToNameMap;
	ShortnameVersionToNameMap m_shortname_to_name_map;

	LocalArchivesVector m_maps_list;	   /// mapname -> hash
	LocalArchivesVector m_mods_list;	   /// modname -> hash
	LocalArchivesVector m_mods_unchained_hash; /// modname -> unchained hash
	LocalArchivesVector m_maps_unchained_hash; /// mapname -> unchained hash
	LocalArchivesVector m_mods_archive_name;   /// modname -> archive name
	LocalArchivesVector m_maps_archive_name;   /// mapname -> archive name
	StringVector m_map_array;		   // this vector is CUSTOM SORTED ALPHABETICALLY, DON'T USE TO ACCESS UNITSYNC DIRECTLY
	StringVector m_mod_array;		   // this vector is CUSTOM SORTED ALPHABETICALLY, DON'T USE TO ACCESS UNITSYNC DIRECTLY
	StringVector m_unsorted_map_array;	 // this is because unitsync doesn't have a search map index by name ..
	StringVector m_unsorted_mod_array;	 // this isn't necessary but makes things more symmetrical :P

	/// caches sett().GetCachePath(), because that method calls back into
	/// susynclib(), there's a good chance main thread blocks on some
	/// WorkerThread operation... cache is invalidated on reload.
	std::string m_cache_path;
	std::map<std::string, GameOptions> m_map_gameoptions;
	std::map<std::string, GameOptions> m_game_gameoptions;

	mutable boost::mutex m_lock;
	WorkerThread* m_cache_thread;
	StringSignalType m_async_ops_complete_sig;

	/// this cache facilitates async image fetching (image is stored in cache
	/// in background thread, then main thread gets it from cache)
	MostRecentlyUsedImageCache m_map_image_cache;
	/// this cache is a real cache, it stores minimaps with max size 100x100
	MostRecentlyUsedImageCache m_tiny_minimap_cache;

	/// this caches MapInfo to facilitate GetMapExAsync
	MostRecentlyUsedMapInfoCache m_mapinfo_cache;

	MostRecentlyUsedArrayStringCache m_sides_cache;

	//! this function returns only the cache path without the file extension,
	//! the extension itself would be added in the function as needed
	std::string GetFileCachePath(const std::string& archivename, bool IsMod, bool usehash = true);

	bool _LoadUnitSyncLib(const std::string& unitsyncloc);
	void _FreeUnitSyncLib();

	MapInfo _GetMapInfoEx(const std::string& mapname);

	void PopulateArchiveList();

	UnitsyncImage _GetMapImage(const std::string& mapname, const std::string& imagename, UnitsyncImage (UnitsyncLib::*loadMethod)(const std::string&));
	UnitsyncImage _GetScaledMapImage(const std::string& mapname, UnitsyncImage (Unitsync::*loadMethod)(const std::string&), int width, int height);

	void _GetMapImageAsync(const std::string& mapname, UnitsyncImage (Unitsync::*loadMethod)(const std::string&));

	friend Unitsync& usync();

public:
	std::string GetNameForShortname(const std::string& shortname, const std::string& version) const;

private:
	//! returns an array where each element is a line of the file
	bool GetCacheFile(const std::string& path, StringVector& ret) const;
	//! write a file where each element of the array is a line
	void SetCacheFile(const std::string& path, const StringVector& data);
};

Unitsync& usync();

struct GameOptions
{
	OptionMapBool bool_map;
	OptionMapFloat float_map;
	OptionMapString string_map;
	OptionMapList list_map;
	OptionMapSection section_map;
};

/// Helper class for managing async operations safely
class UnitSyncAsyncOps : public boost::noncopyable
{
public:
	UnitSyncAsyncOps(const Unitsync::StringSignalSlotType& evtHandler)
	    : m_evtHandler_connection()
	{
		m_evtHandler_connection = usync().RegisterEvtHandler(evtHandler);
	}

	~UnitSyncAsyncOps()
	{
		usync().UnregisterEvtHandler(m_evtHandler_connection);
	}

	void GetMinimap(const std::string& mapname, int w, int h)
	{
		usync().GetMinimapAsync(mapname, w, h);
	}
	void GetMetalmap(const std::string& mapname, int w, int h)
	{
		usync().GetMetalmapAsync(mapname, w, h);
	}
	void GetHeightmap(const std::string& mapname, int w, int h)
	{
		usync().GetHeightmapAsync(mapname, w, h);
	}
	void GetMap(const std::string& mapname)
	{
		usync().GetMapExAsync(mapname);
	}

private:
	boost::signals2::connection m_evtHandler_connection;
};

} // namespace LSL

/**
 * \file unitsync.h
 * \section LICENSE
Copyright 2012 by The libSpringLobby team. All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are
permitted provided that the following conditions are met:

   1. Redistributions of source code must retain the above copyright notice, this list of
      conditions and the following disclaimer.

   2. Redistributions in binary form must reproduce the above copyright notice, this list
      of conditions and the following disclaimer in the documentation and/or other materials
      provided with the distribution.

THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**/

#endif // LIBSPRINGLOBBY_HEADERGUARD_SPRINGUNITSYNC_H
