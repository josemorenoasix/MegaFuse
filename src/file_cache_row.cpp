/*
 * These classes will handle the cached file info.
 */

#include "file_cache_row.h"
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>

#include <megaclient.h>
#include <sys/stat.h>

file_cache_row::file_cache_row(): td(-1), status(INVALID), size(0), available_bytes(0), n_clients(0), startOffset(0), modified(false), handle(0)
{
	char filename[] = "/tmp/mega.XXXXXX";
	close(mkstemp(filename));
	localname = filename;
	printf("Created file: %s\n", localname.c_str());
}
file_cache_row::~file_cache_row()
{

}
/* 
 * Returns true if a read should not block,
 * it doesn't matter if a read will result in an error or in a successful read
 */
bool file_cache_row::canRead(off_t offset,size_t size)
{
	return status != file_cache_row::DOWNLOADING || chunksAvailable(offset,size);
}
off_t file_cache_row::firstUnavailableOffset(bool& ret)
{
	if(status != DOWNLOADING && status != DOWNLOAD_PAUSED)
	{
		ret = true;
		return 0;
	}
	for(size_t i = 0; i < availableChunks.size(); i++)
	{
		if(!availableChunks[i])
		{
			ret = true;
			return CacheManager::blockOffset(i);
		}
	}
	ret = false;
	return 0;
}
/*
 * Tells if the chunks required to perform a read are available
 */
bool file_cache_row::chunksAvailable(off_t startOffset, size_t size)
{
	size_t startChunk = CacheManager::numChunks(ChunkedHash::chunkfloor(startOffset)); // startOffset/CHUNKSIZE
	size_t endChunk = CacheManager::numChunks(startOffset+size);
	bool available = true;
	for(size_t i = startChunk; i < endChunk && i < availableChunks.size(); i++) {
		available = available && availableChunks[i];
	}
	return available;
}
/*
 * Returns the num of chunks required to store a file of this size
 */
size_t CacheManager::numChunks(size_t pos)
{
	size_t end = 0;
	if(pos == 0)
		return 0;
	for(short i=1; i<=8; i++) {
		end += i*ChunkedHash::SEGSIZE;
		if(end >= pos)
			return i;
	}
	return 8 + ceil(float(pos-end)/(8.0*ChunkedHash::SEGSIZE));
}
/*
 * Returns the starting offset of a specified block
 */
size_t CacheManager::blockOffset(size_t pos)
{
	m_off_t end = 0;
	
	for(short i=1; i<=8; i++) {
		if(i > pos)
			return end;
		end +=i*ChunkedHash::SEGSIZE;
	}
	return (pos-8)*8*ChunkedHash::SEGSIZE + end;
}
CacheManager::mapType::iterator CacheManager::findByHandle(uint64_t h)
{
	for(auto it = begin(); it != end(); ++it)
		if(it->second.handle == h)
			return it;
	return end();
}
CacheManager::mapType::iterator CacheManager::findByTransfer(int td, file_cache_row::CacheStatus status)
{
	for(auto it = begin(); it!=end(); ++it)
		if(it->second.status == status && it->second.td == td)
			return it;
	return end();
}
CacheManager::CacheManager()
{

}
file_cache_row& CacheManager::operator[] (std::string s)
{
	return file_cache[s];
}
CacheManager::mapType::iterator CacheManager::find(std::string s)
{
	return file_cache.find(s);
}
CacheManager::mapType::iterator CacheManager::end()
{
	return file_cache.end();
}
CacheManager::mapType::iterator CacheManager::begin()
{
	return file_cache.begin();
}
CacheManager::mapType::const_iterator CacheManager::cend()
{
	return file_cache.cend();
}
CacheManager::mapType::const_iterator CacheManager::cbegin()
{
	return file_cache.cbegin();
}

CacheManager::mapType::iterator CacheManager::tryErase(CacheManager::mapType::iterator it)
{
	if(it->second.n_clients != 0)
		return it;
		
	::unlink(it->second.localname.c_str());
	return file_cache.erase(it);
}

size_t CacheManager::size()
{
	return file_cache.size();
}
