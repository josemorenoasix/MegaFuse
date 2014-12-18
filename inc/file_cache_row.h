#include <string>
#include <vector>
#include <unordered_map>
struct file_cache_row
{
	enum CacheStatus {
		INVALID, DOWNLOADING, UPLOADING, AVAILABLE, DOWNLOAD_PAUSED
	};
	std::string localname;
	std::string tmpname;
	int td;
	CacheStatus status;
	size_t size;
	size_t available_bytes;
	int n_clients;
	time_t last_modified;
	size_t startOffset;
	bool modified;
	std::vector<bool> availableChunks;
	uint64_t handle;
	file_cache_row();
	~file_cache_row();
	
	bool canRead(off_t, size_t);
	bool chunksAvailable(off_t, size_t);
	off_t firstUnavailableOffset(bool&);
};

class CacheManager
{
	public: //only for now
		typedef std::unordered_map <std::string, file_cache_row> mapType;
		mapType file_cache;
	public:
		CacheManager();
		file_cache_row& operator[] (std::string);
		static size_t blockOffset(size_t);
		static size_t numChunks(size_t);
		mapType::iterator findByHandle(uint64_t);
		mapType::iterator findByTransfer(int, file_cache_row::CacheStatus);
		mapType::iterator find(std::string);
		mapType::iterator end();
		mapType::iterator begin();
		mapType::const_iterator cend();
		mapType::const_iterator cbegin();
		mapType::iterator tryErase(mapType::iterator it);
		size_t size();
};