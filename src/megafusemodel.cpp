/* 
 * This is the local representation of the remote fs.
 * It will try to mirror the MEGA remote filesystem.
 */

#include "mega.h"

#include "megacrypto.h"
#include "megaclient.h"
#include "megafusemodel.h"

#include <sys/types.h>
#include <fcntl.h>
#include <sys/stat.h>

std::set<std::string> MegaFuseModel::readdir(const char *path)
{
	std::set<std::string> names;
	
	auto p = splitPath(path);
	std::string p2 = p.first;
	for(auto it = cacheManager.cbegin(); it != cacheManager.cend(); ++it) {
		auto namePair = splitPath(it->first);
		if(namePair.first == std::string(path) && it->second.status != file_cache_row::INVALID)
			names.insert(namePair.second.c_str());
	}
	
	return names;
}
int MegaFuseModel::rename(const char * src, const char *dst)
{
	printf("\033[2KRenaming %s to %s\n", src, dst);
	
	if(cacheManager.find(src) == cacheManager.end() )
		return -ENOENT;

	std::swap(cacheManager[dst],cacheManager[src]);
	std::swap(cacheManager[dst].n_clients,cacheManager[src].n_clients);
	cacheManager[src].status = file_cache_row::INVALID;
	if(cacheManager[src].n_clients <= 0)
		cacheManager.tryErase(cacheManager.find(src));
	return 0;
}
MegaFuseModel::MegaFuseModel(EventsHandler &eh,std::mutex &em):engine_mutex(em), eh(eh), callbacksHandler(this)
{

}
int MegaFuseModel::getAttr(const char *path, struct stat *stbuf)
{
	for(auto it = cacheManager.cbegin(); it != cacheManager.cend(); ++it)
	{
		if(it->first == std::string(path))
		{
			stbuf->st_mode = S_IFREG | 0666;
			stbuf->st_nlink = 1;
			
			struct stat st;
			stbuf->st_size = it->second.size;
			stbuf->st_gid = getgid();
			stbuf->st_uid = getuid();
			stbuf->st_mtime = it->second.last_modified;
			return 0;
		}
	}
	
	return -ENOENT;
}
std::pair<std::string,std::string> MegaFuseModel::splitPath(std::string path)
{
	int pos = path.find_last_of('/');
	std::string basename = path.substr(0, pos);
	std::string filename = path.substr(pos+1);
	if(basename == "")
		basename = "/";

	return std::pair<std::string,std::string> (basename, filename);
}
// Warning, no mutex
Node* MegaFuseModel::nodeByPath(std::string path)
{
	if(engine_mutex.try_lock()) {
		printf("\033[2KMutex Error in MegaFuseModel::nodeByPath\n");
		abort();
	}
	if (path == "/") {
		Node*n = client->nodebyhandle(client->rootnodes[0]);
		n->type = ROOTNODE;
		return n;
	}
	if(path.at(0) == '/')
		path = path.substr(1);
	Node *n = client->nodebyhandle(client->rootnodes[0]);
	
	int pos;
	while((pos = path.find_first_of('/')) > 0) {
		n = childNodeByName(n, path.substr(0, pos));
		if(!n)
		{
			return NULL;
		}

		path = path.substr(pos+1);
	}
	n = childNodeByName(n, path);
	if(!n)
	{
		return NULL;
	}

	return n;
}
Node* MegaFuseModel::childNodeByName(Node *p, std::string name)
{
	if(engine_mutex.try_lock()) {
		printf("\033[2KMutex Error in MegaFuseModel::childNodeByName\n");
		abort();
	}
	for (node_list::iterator it = p->children.begin(); it != p->children.end(); it++) {
		if (!strcmp(name.c_str(), (*it)->displayname())) {
			return *it;
		}
	}

	return NULL;
}
void MegaFuseModel::check_cache()
{
	if(cacheManager.size() < 2)
		return;

	printf("\033[2KChecking cache...\n");
	std::lock_guard<std::mutex>lock(engine_mutex);
	for(auto it = cacheManager.begin(); it!= cacheManager.end(); ++it) {
		if(it->second.n_clients==0 && it->second.status ==file_cache_row::AVAILABLE) {
			printf("\033[2KRemoving file %s from cache.\n",it->first.c_str());
			if(it->second.status ==file_cache_row::DOWNLOADING) //UPLOADING FILES IGNORED
				client->tclose(it->second.td);
			cacheManager.tryErase(it);
		}
	}
}
void createthumbnail(const char* filename, unsigned size, string* result);
MegaApp* MegaFuseModel::getCallbacksHandler()
{
	return &callbacksHandler;
}
int MegaFuseModel::release(const char *path, struct fuse_file_info *fi)
{
	int ret = 0;
	auto it = cacheManager.begin();
	
	std::unique_lock<std::mutex> lock(engine_mutex);
	it = cacheManager.find(std::string(path));
	it->second.n_clients--;
	
	if(!it->second.n_clients && it->second.modified) {
		Node *oldNode = nodeByPath(it->first);
		
		auto target = splitPath(it->first);
		
		int td = client->topen(it->second.localname.c_str(), -1, 2);
		if (td < 0)
			return -EAGAIN;
		it->second.td = td;
		it->second.status = file_cache_row::UPLOADING;
		
		std::string thumbnail;
		createthumbnail(it->second.localname.c_str(), 120, &thumbnail);
		
		if (thumbnail.size()) {
			printf("\033[2KImage detected and thumbnail extracted, size %s bytes\n", thumbnail.size());
			handle uh = client->uploadhandle(td);
			client->putfa(&client->ft[td].key, uh, THUMBNAIL120X120, (const byte*)thumbnail.data(), thumbnail.size());
		}
		
		if(it->second.status == file_cache_row::UPLOADING) {
			EventsListener el(eh, EventsHandler::UPLOAD_COMPLETE);
			EventsListener el2(eh, EventsHandler::NODE_UPDATED);
			lock.unlock();
			auto l_ress = el.waitEvent();
			if(l_ress.result < 0)
			{
				printf("\033[2KError while uploading the file: %s\n", errorstring(error(l_ress.result)));
				return -EIO;
			}

			auto l_res = el2.waitEvent();
			if(oldNode) {
				lock.lock();
				client->unlink(oldNode);
				lock.unlock();
			}
		}
	}
	return ret;
}
int MegaFuseModel::open(const char *p, struct fuse_file_info *fi)
{
	auto sPath = splitPath(p);
	std::string path(p);
	// printf("flags: %X\n", fi->flags);
	
	// Workaround for kde bug in debian 7
	if(sPath.second.find(".directory") == 0 && (fi->flags & O_EXCL))
		return -EPERM;
		
	std::unique_lock <std::mutex> engine(engine_mutex);
	Node *n = nodeByPath(p);
	auto it = cacheManager.find(path);
	if(it != cacheManager.end()  && it->second.status == file_cache_row::DOWNLOAD_PAUSED) {
		printf("\033[2KResuming paused download\n");
	}
	// Files with 0 clients are OK, it's a cache
	bool oldCache = it != cacheManager.end() && n && it->second.last_modified < n->mtime;
	
	if(oldCache && it->second.n_clients > 0)
		return -EAGAIN;
	if(oldCache && it->second.n_clients <= 0) {
		cacheManager.tryErase(it);
		it = cacheManager.end();
	}
	
	bool fileExists = n || it != cacheManager.end();	
	if(!fileExists && !(fi->flags & O_CREAT))
		return -ENOENT;
	if(fileExists && (fi->flags & O_CREAT) && (fi->flags & O_EXCL))
		return -EEXIST;
		
	if(fi->flags & O_TRUNC || (!fileExists && (fi->flags & O_CREAT)) ) {
		if(cacheManager[p].status == file_cache_row::DOWNLOADING) {
			client->tclose(cacheManager[p].td);
			cacheManager[p].td = -1;
		}
		cacheManager[p].status 		= file_cache_row::AVAILABLE;
		cacheManager[p].size 		= 0;
		cacheManager[p].modified 	= true;
		cacheManager[p].n_clients++;
		
		truncate(cacheManager[p].localname.c_str(), 0);
		return 0;
	}
	
	if(it != cacheManager.end() && it->second.status != file_cache_row::INVALID && it->second.status != file_cache_row::DOWNLOAD_PAUSED) {
		it->second.n_clients++;
		return 0;
	}
	if(it != cacheManager.end() && it->second.status == file_cache_row::DOWNLOAD_PAUSED) {
		printf("\033[2KResuming paused download\n");
	} else {
		cacheManager[p].status = file_cache_row::INVALID;
		it = cacheManager.find(p);
		
	}
	engine.unlock();
	if(it == cacheManager.end())
		cacheManager[p].status = file_cache_row::INVALID;
	int res = makeAvailableForRead(p,0,0);
	if(res < 0)
		return res;
		
	engine.lock();
	it->second.n_clients++;
	it->second.handle = n->nodehandle;
	return 0;
}
int MegaFuseModel::write(const char * path, const char *buf, size_t size, off_t offset, struct fuse_file_info * fi)
{
	auto it = cacheManager.find(path);
	
	chmod(it->second.localname.c_str(),S_IWUSR|S_IRUSR);
	printf("\033[2KWrite, file %s, open cache: %s\n", it->first.c_str(), it->second.localname.c_str());
	int fd = ::open(it->second.localname.c_str(),O_WRONLY);
	if (fd < 0)
		return fd;
	int s = pwrite(fd,buf,size,offset);
	close(fd);
	it->second.modified=true;
	int newsize = offset + size;
	if(it->second.size < newsize) {
		it->second.size = newsize;
		it->second.availableChunks.resize(CacheManager::numChunks(it->second.size), false);
	}
	return s;
}
int MegaFuseModel::makeAvailableForRead(const char *path, off_t offset, size_t size) // 0 is success
{
	std::unique_lock<std::mutex> engine(engine_mutex);
	
	/* 
	 * Cases:
	 * AVAILABLE or uploading = return true
	 * INVALID: 			stop = false, 	start = true
	 * DOWNLOAD_PAUSED: 	stop = false, 	start = true
	 * DOWNLOADING near: stop = false, 	start = false
	 * DOWNLOADING far: 	stop = true, 	start = true
	 *
	 * 3 sections, stop, start and wait
	 * */
	auto it = cacheManager.find(path);
	if(it == cacheManager.end())
		return -EINVAL;
		
	if(it->second.status == file_cache_row::UPLOADING || it->second.status == file_cache_row::AVAILABLE)
		return 0;
		
	bool stopFirst = false;
	bool startDownload = true;
	
	switch(it->second.status) {
		case file_cache_row::INVALID:
			break;
		case file_cache_row::DOWNLOADING:
			if(it->second.chunksAvailable(offset, size))
				return 0;
			if(((offset+size) - (it->second.available_bytes)) < 2*1024*1024 && it->second.startOffset <= offset)
				startDownload = false;
			else
				stopFirst = true;
			break;
		case file_cache_row::DOWNLOAD_PAUSED:
		
			break;
		case file_cache_row::UPLOADING:
		case file_cache_row::AVAILABLE:
			return 0;
	}
	
	if(stopFirst) {
		client->tclose(it->second.td);
		it->second.td = -1;
	}
	
	if(startDownload) {
		Node * n = nodeByPath(path);
		if(!n)
			return -EINVAL;
		bool ret = true;
		
		off_t startOffset = (it->second.status == file_cache_row::DOWNLOAD_PAUSED)?
			it->second.firstUnavailableOffset(ret):
			ChunkedHash::chunkfloor(offset);
		int td;
		if(ret)
			td = client->topen(n->nodehandle, NULL, startOffset, -1, 1);
		else
			td = client->topen(n->nodehandle, NULL, -1, -1, 1);

		if(td < 0)
		{
			printf("\033[2KError while opening the file: %s\n", errorstring(error(td)))	;
			return -EIO;
		}	

		it->second.td = td;
		it->second.size = n->size;
		EventsListener el(eh,EventsHandler::TOPEN_RESULT);
		engine.unlock();
		auto open_result = el.waitEvent();
		if(open_result.result < 0)
		{
			printf("\033[2KError while waiting for topen_result the file: %s\n", errorstring(error(open_result.result)))	;
			if(open_result.result == API_ETEMPUNAVAIL)
				return -EAGAIN;
			return -EIO;
		}	

		engine.lock();
		it->second.last_modified = n->mtime;
		
		it->second.startOffset = startOffset;
		it->second.available_bytes = 0;
	}
	
	EventsListener el(eh,EventsHandler::TRANSFER_UPDATE);
	while(!it->second.canRead(offset,size)) {
		engine.unlock();
		el.waitEvent();
		engine.lock();
	}

	return 0;
}
int MegaFuseModel::read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
	std::unique_lock<std::mutex> engine(engine_mutex);
	
	int fd = ::open(cacheManager[path].localname.c_str(),O_RDONLY);
	if (fd < 0 ) {
		printf("\033[2KFailed to open the file in read: %s\n", cacheManager[path].localname.c_str());
		return -EIO;
	}
	int s = pread(fd,buf,size,offset);
	close(fd);
	return s;
}
int MegaFuseModel::unlink(std::string filename)
{
	printf("\033[2KDeleting file: %s\n", filename.c_str());
	
	auto it = cacheManager.find(filename);
	if(it == cacheManager.end())
		return -ENOENT;
		
	if( it->second.td > -1)
		client->tclose(it->second.td);
		
	it->second.status = file_cache_row::INVALID;
	if( it->second.n_clients <= 0)
		cacheManager.tryErase(it);
	return 0;
}
MegaFuseModel::~MegaFuseModel()
{
	auto it = cacheManager.begin();
	while(it != cacheManager.end())
		it = cacheManager.tryErase(it);
}
