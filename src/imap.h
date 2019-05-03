// imap.h
//
// Copyright (c) 2019 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#pragma once

#include <map>
#include <mutex>
#include <set>
#include <string>

#include "body.h"
#include "header.h"

class Imap
{
public:
  Imap(const std::string& p_User, const std::string& p_Pass, const std::string& p_Host,
       const uint16_t p_Port, const bool p_CacheEncrypt);
  virtual ~Imap();
  
  bool Login();
  bool Logout();

  std::set<std::string> GetFolders(bool p_Cached);
  std::set<uint32_t> GetUids(const std::string& p_Folder, bool p_Cached);
  std::map<uint32_t, Header> GetHeaders(const std::string& p_Folder,
                                        const std::set<uint32_t>& p_Uids, bool p_Cached);
  std::map<uint32_t, uint32_t> GetFlags(const std::string& p_Folder,
                                        const std::set<uint32_t>& p_Uids, bool p_Cached);
  std::map<uint32_t, Body> GetBodys(const std::string& p_Folder,
                                    const std::set<uint32_t>& p_Uids, bool p_Cached);

  bool SetFlagSeen(const std::string& p_Folder, const std::set<uint32_t>& p_Uids, bool p_Value);
  bool SetFlagDeleted(const std::string& p_Folder, const std::set<uint32_t>& p_Uids,
                      bool p_Value);
  bool MoveMessages(const std::string& p_Folder, const std::set<uint32_t>& p_Uids,
                    const std::string& p_DestFolder);
  bool DeleteMessages(const std::string& p_Folder, const std::set<uint32_t>& p_Uids);

  bool GetConnected();
  int IdleStart(const std::string& p_Folder);
  void IdleDone();

private:
  bool SelectFolder(const std::string& p_Folder, bool p_Force = false);
  uint32_t GetUidValidity();
  std::string GetCacheDir();
  void InitCacheDir();
  std::string GetImapCacheDir();
  std::string GetFolderCacheDir(const std::string& p_Folder);
  std::string GetFolderUidsCachePath(const std::string& p_Folder);
  std::string GetFolderFlagsCachePath(const std::string& p_Folder);
  std::string GetFoldersCachePath();
  std::string GetMessageCachePath(const std::string& p_Folder, uint32_t p_Uid,
                                  const std::string& p_Suffix);
  std::string GetHeaderCachePath(const std::string& p_Folder, uint32_t p_Uid);
  std::string GetBodyCachePath(const std::string& p_Folder, uint32_t p_Uid);

  void InitFolderCacheDir(const std::string& p_Folder);
  void CommonInitCacheDir(const std::string& p_Dir, int p_Version);

  std::string ReadCacheFile(const std::string& p_Path);
  void WriteCacheFile(const std::string& p_Path, const std::string& p_Str);

private:
  std::string m_User;
  std::string m_Pass;
  std::string m_Host;
  uint16_t m_Port = 0;
  bool m_CacheEncrypt = false;

  std::mutex m_ImapMutex;
	struct mailimap* m_Imap = NULL;

  std::string m_SelectedFolder;

  std::mutex m_ConnectedMutex;
  bool m_Connected = false;
};