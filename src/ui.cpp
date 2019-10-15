// ui.cpp
//
// Copyright (c) 2019 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#include "ui.h"

#include <algorithm>
#include <climits>
#include <cstdint>
#include <sstream>

#include "addressbook.h"
#include "flag.h"
#include "loghelp.h"
#include "maphelp.h"
#include "sethelp.h"
#include "status.h"

Ui::Ui(const std::string& p_Inbox, const std::string& p_Address, uint32_t p_PrefetchLevel)
  : m_Inbox(p_Inbox)
  , m_Address(p_Address)
  , m_PrefetchLevel(p_PrefetchLevel)
{
  m_CurrentFolder = p_Inbox;
  Init();
  InitWindows();
}

Ui::~Ui()
{
  CleanupWindows();
  Cleanup();
}

void Ui::Init()
{
  signal(SIGINT, SIG_IGN);
  setlocale(LC_ALL, "");
  initscr();
  noecho();
  cbreak();
  raw();
  keypad(stdscr, TRUE);
  curs_set(0);
  pipe(m_Pipe);

  const std::map<std::string, std::string> defaultConfig =
  {
    {"help_enabled", "1"},
    {"persist_folder_filter", "1"},
    {"plain_text", "1"},
    {"show_progress", "1"},
    {"key_prev_msg", "p"},
    {"key_next_msg", "n"},
    {"key_reply", "r"},
    {"key_forward", "f"},
    {"key_delete", "d"},
    {"key_compose", "c"},
    {"key_toggle_unread", "u"},
    {"key_move", "m"},
    {"key_refresh", "l"},
    {"key_quit", "q"},
    {"key_toggle_text_html", "t"},
    {"key_cancel", "KEY_CTRLC"},
    {"key_send", "KEY_CTRLX"},
    {"key_delete_line", "KEY_CTRLK"},
    {"key_open", "."},
    {"key_back", ","},
    {"key_goto_folder", "g"},
    {"key_address_book", "KEY_CTRLT"},
    {"key_save_file", "s"},
  };
  const std::string configPath(Util::GetApplicationDir() + std::string("ui.conf"));
  m_Config = Config(configPath, defaultConfig);

  m_HelpEnabled = m_Config.Get("help_enabled") == "1";
  m_PersistFolderFilter = m_Config.Get("persist_folder_filter") == "1";
  m_Plaintext = m_Config.Get("plain_text") == "1";
  m_KeyPrevMsg = Util::GetKeyCode(m_Config.Get("key_prev_msg"));
  m_KeyNextMsg = Util::GetKeyCode(m_Config.Get("key_next_msg"));
  m_KeyReply = Util::GetKeyCode(m_Config.Get("key_reply"));
  m_KeyForward = Util::GetKeyCode(m_Config.Get("key_forward"));
  m_KeyDelete = Util::GetKeyCode(m_Config.Get("key_delete"));
  m_KeyCompose = Util::GetKeyCode(m_Config.Get("key_compose"));
  m_KeyToggleUnread = Util::GetKeyCode(m_Config.Get("key_toggle_unread"));
  m_KeyMove = Util::GetKeyCode(m_Config.Get("key_move"));
  m_KeyRefresh = Util::GetKeyCode(m_Config.Get("key_refresh"));
  m_KeyQuit = Util::GetKeyCode(m_Config.Get("key_quit"));
  m_KeyToggleTextHtml = Util::GetKeyCode(m_Config.Get("key_toggle_text_html"));
  m_KeyCancel = Util::GetKeyCode(m_Config.Get("key_cancel"));
  m_KeySend = Util::GetKeyCode(m_Config.Get("key_send"));
  m_KeyDeleteLine = Util::GetKeyCode(m_Config.Get("key_delete_line"));
  m_KeyOpen = Util::GetKeyCode(m_Config.Get("key_open"));
  m_KeyBack = Util::GetKeyCode(m_Config.Get("key_back"));
  m_KeyGotoFolder = Util::GetKeyCode(m_Config.Get("key_goto_folder"));
  m_KeyAddressBook = Util::GetKeyCode(m_Config.Get("key_address_book"));
  m_KeySaveFile = Util::GetKeyCode(m_Config.Get("key_save_file"));
  m_ShowProgress = m_Config.Get("show_progress") == "1";
}

void Ui::Cleanup()
{
  m_Config.Set("plain_text", m_Plaintext ? "1" : "0");
  m_Config.Save();
  close(m_Pipe[0]);
  close(m_Pipe[1]);
  wclear(stdscr);
  endwin();
}

void Ui::InitWindows()
{
  getmaxyx(stdscr, m_ScreenHeight, m_ScreenWidth);
  wclear(stdscr);
  wrefresh(stdscr);
  const int topHeight = 1;
  m_TopWin = newwin(topHeight, m_ScreenWidth, 0, 0);
  leaveok(m_TopWin, true);

  int helpHeight = 0;
  if (m_HelpEnabled)
  {
    helpHeight = 2;
    m_HelpWin = newwin(2, m_ScreenWidth, m_ScreenHeight - helpHeight, 0);
    leaveok(m_HelpWin, true);
  }

  const int dialogHeight = 1;
  m_DialogWin = newwin(1, m_ScreenWidth, m_ScreenHeight - helpHeight - dialogHeight, 0);
  leaveok(m_DialogWin, true);

  bool listPad = true;
  if (listPad)
  {
    m_MainWinHeight = m_ScreenHeight - topHeight - helpHeight - 2;
    m_MainWin = newwin(m_MainWinHeight, m_ScreenWidth, topHeight + 1, 0);
  }
  else
  {
    m_MainWinHeight = m_ScreenHeight - topHeight - helpHeight;
    m_MainWin = newwin(m_MainWinHeight, m_ScreenWidth, topHeight, 0);
  }

  leaveok(m_MainWin, true);
}

void Ui::CleanupWindows()
{
  delwin(m_TopWin);
  m_TopWin = NULL;
  delwin(m_MainWin);
  m_MainWin = NULL;
  delwin(m_DialogWin);
  m_DialogWin = NULL;
  if (m_HelpWin != NULL)
  {
    delwin(m_HelpWin);
    m_HelpWin = NULL;
  }
}

void Ui::DrawAll()
{
  switch (m_State)
  {
    case StateViewMessageList:
      DrawTop();
      DrawMessageList();
      DrawHelp();
      DrawDialog();
      break;

    case StateViewMessage:
      DrawTop();
      DrawMessage();
      DrawHelp();
      DrawDialog();
      break;

    case StateGotoFolder:
    case StateMoveToFolder:
      DrawTop();
      DrawFolderList();
      DrawHelp();
      DrawDialog();
      break;

    case StateComposeMessage:
    case StateReplyMessage:
    case StateForwardMessage:
      DrawTop();
      DrawHelp();
      DrawDialog();
      DrawComposeMessage();
      break;

    case StateAddressList:
      DrawTop();
      DrawAddressList();
      DrawHelp();
      DrawDialog();
      break;      

    case StateViewPartList:
      DrawTop();
      DrawPartList();
      DrawHelp();
      DrawDialog();
      break;

    default:
      werase(m_MainWin);
      mvwprintw(m_MainWin, 0, 0, "Unimplemented state %d", m_State);
      wrefresh(m_MainWin);
      break;
  }
}

void Ui::DrawTop()
{
  werase(m_TopWin);
  wattron(m_TopWin, A_REVERSE);

  std::string version = "  nmail " + Util::GetAppVersion();
  std::string topLeft = Util::TrimPadString(version, (m_ScreenWidth - 13) / 2);
  std::string status = GetStatusStr();
  std::string topRight = status + "  ";
  std::string topCenter = Util::TrimPadString(GetStateStr(),
                                              m_ScreenWidth - topLeft.size() - topRight.size());
  std::string topCombined = topLeft + topCenter + topRight;

  mvwprintw(m_TopWin, 0, 0, "%s", topCombined.c_str());
  wattroff(m_TopWin, A_NORMAL);
  wrefresh(m_TopWin);
}

void Ui::DrawDialog()
{
  switch (m_State)
  {
    case StateGotoFolder:
    case StateMoveToFolder:
    case StateAddressList:
      DrawSearchDialog();
      break;

    default:
      DrawDefaultDialog();
      break;
  }
}

void Ui::DrawSearchDialog()
{
  int filterPos = 0;
  std::wstring filterStr;
  
  switch (m_State)
  {
    case StateGotoFolder:
    case StateMoveToFolder:
      filterPos = m_FolderListFilterPos;
      filterStr = m_FolderListFilterStr;
      break;

    case StateAddressList:
      filterPos = m_AddressListFilterPos;
      filterStr = m_AddressListFilterStr;
      break;

    default:
      break;
  }
  
  werase(m_DialogWin);
  const std::string& dispStr = Util::ToString(filterStr);
  mvwprintw(m_DialogWin, 0, 0, "   Search: %s", dispStr.c_str());

  leaveok(m_DialogWin, false);
  wmove(m_DialogWin, 0, 11 + filterPos);
  wrefresh(m_DialogWin);
  leaveok(m_DialogWin, true);
}

void Ui::DrawDefaultDialog()
{
  werase(m_DialogWin);

  {
    std::lock_guard<std::mutex> lock(m_Mutex);
    std::chrono::time_point<std::chrono::system_clock> nowTime =
      std::chrono::system_clock::now();
    std::chrono::duration<double> elapsed = nowTime - m_DialogMessageTime;
    if ((elapsed.count() < 1.0f) && !m_DialogMessage.empty())
    {
      int x = std::max((m_ScreenWidth - (int)m_DialogMessage.size() - 1) / 2, 0);
      const std::string& dispStr = m_DialogMessage;
      wattron(m_DialogWin, A_REVERSE);
      mvwprintw(m_DialogWin, 0, x, " %s ", dispStr.c_str());
      wattroff(m_DialogWin, A_REVERSE);
    }
  }

  wrefresh(m_DialogWin);
}

void Ui::SetDialogMessage(const std::string &p_DialogMessage)
{
  std::lock_guard<std::mutex> lock(m_Mutex);
  m_DialogMessage = p_DialogMessage;
  m_DialogMessageTime = std::chrono::system_clock::now();
}

void Ui::DrawHelp()
{
  static std::vector<std::vector<std::string>> viewMessagesListHelp =
  {
    {
      GetKeyDisplay(m_KeyBack), "Folders",
      GetKeyDisplay(m_KeyPrevMsg), "PrevMsg",
      GetKeyDisplay(m_KeyReply), "Reply",
      GetKeyDisplay(m_KeyDelete), "Delete",
      GetKeyDisplay(m_KeyToggleUnread), "TgUnread",
      GetKeyDisplay(m_KeyRefresh), "Refresh",
    },
    {
      GetKeyDisplay(m_KeyOpen), "ViewMsg",
      GetKeyDisplay(m_KeyNextMsg), "NextMsg",
      GetKeyDisplay(m_KeyForward), "Forward",
      GetKeyDisplay(m_KeyCompose), "Compose",
      GetKeyDisplay(m_KeyMove), "Move",
      GetKeyDisplay(m_KeyQuit), "Quit",
    },
  };

  static std::vector<std::vector<std::string>> viewMessageHelp =
  {
    {
      GetKeyDisplay(m_KeyBack), "MsgList",
      GetKeyDisplay(m_KeyPrevMsg), "PrevMsg",
      GetKeyDisplay(m_KeyReply), "Reply",
      GetKeyDisplay(m_KeyDelete), "Delete",
      GetKeyDisplay(m_KeyToggleUnread), "TgUnread",
      GetKeyDisplay(m_KeyToggleTextHtml), "TgTxtHtml",
    },
    {
      GetKeyDisplay(m_KeyOpen), "MsgParts",
      GetKeyDisplay(m_KeyNextMsg), "NextMsg",
      GetKeyDisplay(m_KeyForward), "Forward",
      GetKeyDisplay(m_KeyCompose), "Compose",
      GetKeyDisplay(m_KeyMove), "Move",
      GetKeyDisplay(m_KeyQuit), "Quit",
    }
  };

  static std::vector<std::vector<std::string>> viewFoldersHelp =
  {
    {
      GetKeyDisplay(KEY_RETURN), "Select",
    },
    {
      GetKeyDisplay(m_KeyCancel), "Cancel",
    }
  };

  static std::vector<std::vector<std::string>> composeMessageHelp =
  {
    {
      GetKeyDisplay(m_KeySend), "Send",
      GetKeyDisplay(m_KeyDeleteLine), "DelLine",
    },
    {
      GetKeyDisplay(m_KeyCancel), "Cancel",
      GetKeyDisplay(m_KeyAddressBook), "AddrBk",
    },
  };

  static std::vector<std::vector<std::string>> viewPartListHelp =
  {
    {
      GetKeyDisplay(m_KeyBack), "ViewMsg",
      GetKeyDisplay(m_KeyPrevMsg), "PrevPart",
      GetKeyDisplay(m_KeySaveFile), "Save",
      "", "",
      "", "",
      "", "",
    },
    {
      GetKeyDisplay(m_KeyOpen), "ViewPart",
      GetKeyDisplay(m_KeyNextMsg), "NextPart",
      GetKeyDisplay(m_KeyQuit), "Quit",
      "", "",
      "", "",
      "", "",
    },
  };

  if (m_HelpEnabled)
  {
    werase(m_HelpWin);
    switch (m_State)
    {
      case StateViewMessageList:
        DrawHelpText(viewMessagesListHelp);
        break;

      case StateViewMessage:
        DrawHelpText(viewMessageHelp);
        break;

      case StateGotoFolder:
      case StateMoveToFolder:
      case StateAddressList:
        DrawHelpText(viewFoldersHelp);
        break;

      case StateComposeMessage:
      case StateReplyMessage:
      case StateForwardMessage:
        DrawHelpText(composeMessageHelp);
        break;

      case StateViewPartList:
        DrawHelpText(viewPartListHelp);
        break;

      default:
        break;
    }

    wrefresh(m_HelpWin);
  }
}

void Ui::DrawHelpText(const std::vector<std::vector<std::string> > &p_HelpText)
{
  int cols = 6;
  int width = m_ScreenWidth / cols;

  int y = 0;
  for (auto rowIt = p_HelpText.begin(); rowIt != p_HelpText.end(); ++rowIt)
  {
    int x = 0;
    for (int colIdx = 0; colIdx < (int)rowIt->size(); colIdx += 2)
    {
      std::string cmd = rowIt->at(colIdx);
      std::string desc = rowIt->at(colIdx + 1);

      wattron(m_HelpWin, A_REVERSE);
      mvwprintw(m_HelpWin, y, x, "%s", cmd.c_str());
      wattroff(m_HelpWin, A_REVERSE);

      const std::string descTrim = desc.substr(0, width - cmd.size() - 2);
      mvwprintw(m_HelpWin, y, x + cmd.size() + 1, "%s", descTrim.c_str());

      x += width;
    }

    ++y;
  }
}

void Ui::DrawFolderList()
{
  if (!m_HasRequestedFolders)
  {
    ImapManager::Request request;
    request.m_GetFolders = true;
    LOG_DEBUG("request folders");
    m_HasRequestedFolders = true;
    m_ImapManager->AsyncRequest(request);
  }

  werase(m_MainWin);

  std::set<std::string> folders;

  if (m_FolderListFilterStr.empty())
  {
    std::lock_guard<std::mutex> lock(m_Mutex);
    folders = m_Folders;
  }
  else
  {
    std::lock_guard<std::mutex> lock(m_Mutex);
    for (const auto& folder : m_Folders)
    {
      if (Util::ToLower(folder).find(Util::ToLower(Util::ToString(m_FolderListFilterStr)))
          != std::string::npos)
      {
        folders.insert(folder);
      }
    }
  }

  int count = folders.size();
  if (count > 0)
  {
    if (m_FolderListCurrentIndex == INT_MAX)
    {
      for (int i = 0; i < count; ++i)
      {
        const std::string& folder = *std::next(folders.begin(), i);
        if (folder == m_CurrentFolder)
        {
          m_FolderListCurrentIndex = i;
        }
      }
    }

    m_FolderListCurrentIndex = Util::Bound(0, m_FolderListCurrentIndex, (int)folders.size() - 1);

    int itemsMax = m_MainWinHeight - 1;
    int idxOffs = Util::Bound(0, (int)(m_FolderListCurrentIndex - ((itemsMax - 1) / 2)),
                              std::max(0, (int)folders.size() - (int)itemsMax));
    int idxMax = idxOffs + std::min(itemsMax, (int)folders.size());

    for (int i = idxOffs; i < idxMax; ++i)
    {
      const std::string& folder = *std::next(folders.begin(), i);

      if (i == m_FolderListCurrentIndex)
      {
        wattron(m_MainWin, A_REVERSE);
        m_FolderListCurrentFolder = folder;
      }

      std::wstring wfolder = Util::ToWString(folder);
      mvwaddnwstr(m_MainWin, i - idxOffs, 2, wfolder.c_str(), wfolder.size());

      if (i == m_FolderListCurrentIndex)
      {
        wattroff(m_MainWin, A_REVERSE);
      }
    }
  }

  wrefresh(m_MainWin);
}

void Ui::DrawAddressList()
{
  werase(m_MainWin);

  std::vector<std::string> addresses;

  if (m_AddressListFilterStr.empty())
  {
    addresses = m_Addresses;
  }
  else
  {
    for (const auto& address : m_Addresses)
    {
      if (Util::ToLower(address).find(Util::ToLower(Util::ToString(m_AddressListFilterStr)))
          != std::string::npos)
      {
        addresses.push_back(address);
      }
    }
  }

  int count = addresses.size();
  if (count > 0)
  {
    m_AddressListCurrentIndex = Util::Bound(0, m_AddressListCurrentIndex, (int)addresses.size() - 1);

    int itemsMax = m_MainWinHeight - 1;
    int idxOffs = Util::Bound(0, (int)(m_AddressListCurrentIndex - ((itemsMax - 1) / 2)),
                              std::max(0, (int)addresses.size() - (int)itemsMax));
    int idxMax = idxOffs + std::min(itemsMax, (int)addresses.size());

    for (int i = idxOffs; i < idxMax; ++i)
    {
      const std::string& address = *std::next(addresses.begin(), i);

      if (i == m_AddressListCurrentIndex)
      {
        wattron(m_MainWin, A_REVERSE);
        m_AddressListCurrentAddress = address;
      }

      std::wstring waddress = Util::ToWString(address);
      mvwaddnwstr(m_MainWin, i - idxOffs, 2, waddress.c_str(), waddress.size());

      if (i == m_AddressListCurrentIndex)
      {
        wattroff(m_MainWin, A_REVERSE);
      }
    }
  }

  wrefresh(m_MainWin);
}

void Ui::DrawMessageList()
{
  if (!m_HasRequestedUids[m_CurrentFolder])
  {
    ImapManager::Request request;
    request.m_Folder = m_CurrentFolder;
    request.m_GetUids = true;
    LOG_DEBUG_VAR("request uids =", m_CurrentFolder);
    m_HasRequestedUids[m_CurrentFolder] = true;
    m_ImapManager->AsyncRequest(request);
  }
  
  std::set<uint32_t> fetchHeaderUids;  
  std::set<uint32_t> fetchFlagUids;  
  std::set<uint32_t> prefetchBodyUidsPrimary;
  std::set<uint32_t> prefetchBodyUidsSecondary;

  {
    std::lock_guard<std::mutex> lock(m_Mutex);
    std::set<uint32_t>& newUids = m_NewUids[m_CurrentFolder];
    std::map<uint32_t, Header>& headers = m_Headers[m_CurrentFolder];
    std::map<uint32_t, uint32_t>& flags = m_Flags[m_CurrentFolder];
    auto& msgDateUids = m_MsgDateUids[m_CurrentFolder];

    std::set<uint32_t>& requestedHeaders = m_RequestedHeaders[m_CurrentFolder];
    std::set<uint32_t>& requestedFlags = m_RequestedFlags[m_CurrentFolder];

    if (!newUids.empty())
    {
      for (auto& uid : newUids)
      {
        if ((headers.find(uid) == headers.end()) &&
            (requestedHeaders.find(uid) == requestedHeaders.end()))
        {
          fetchHeaderUids.insert(uid);
          requestedHeaders.insert(uid);
        }

        if ((flags.find(uid) == flags.end()) &&
            (requestedFlags.find(uid) == requestedFlags.end()))
        {
          fetchFlagUids.insert(uid);
          requestedFlags.insert(uid);
        }
      }

      newUids.clear();
    }

    const std::map<uint32_t, Body>& bodys = m_Bodys[m_CurrentFolder];
    std::set<uint32_t>& prefetchedBodys = m_PrefetchedBodys[m_CurrentFolder];
    std::set<uint32_t>& requestedBodys = m_RequestedBodys[m_CurrentFolder];
    
    int idxOffs = Util::Bound(0, (int)(m_MessageListCurrentIndex[m_CurrentFolder] -
                                       ((m_MainWinHeight - 1) / 2)),
                              std::max(0, (int)msgDateUids.size() - (int)m_MainWinHeight));
    int idxMax = idxOffs + std::min(m_MainWinHeight, (int)msgDateUids.size());

    werase(m_MainWin);

    for (int i = idxOffs; i < idxMax; ++i)
    {
      uint32_t uid = std::prev(msgDateUids.end(), i + 1)->second;

      std::string seenFlag;
      if ((flags.find(uid) != flags.end()) && (!Flag::GetSeen(flags.at(uid))))
      {
        seenFlag = std::string("N");
      }

      std::string shortDate;
      std::string shortFrom;
      std::string subject;
      if (headers.find(uid) != headers.end())
      {
        Header& header = headers.at(uid);
        shortDate = header.GetShortDate();
        shortFrom = header.GetShortFrom();
        subject = header.GetSubject();
      }

      seenFlag = Util::TrimPadString(seenFlag, 1);
      shortDate = Util::TrimPadString(shortDate, 10);
      shortFrom = Util::ToString(Util::TrimPadWString(Util::ToWString(shortFrom), 20));
      std::string headerLeft = " " + seenFlag + "  " + shortDate + "  " + shortFrom + "  ";
      int subjectWidth = m_ScreenWidth - headerLeft.size() - 1;
      subject = Util::ToString(Util::TrimPadWString(Util::ToWString(subject), subjectWidth));
      std::string header = headerLeft + subject + " ";

      if (i == m_MessageListCurrentIndex[m_CurrentFolder])
      {
        wattron(m_MainWin, A_REVERSE);
      }

      std::wstring wheader = Util::ToWString(header);
      mvwaddnwstr(m_MainWin, i - idxOffs, 0, wheader.c_str(), wheader.size());

      if (i == m_MessageListCurrentIndex[m_CurrentFolder])
      {
        wattroff(m_MainWin, A_REVERSE);
      }

      if (i == m_MessageListCurrentIndex[m_CurrentFolder])
      {
        if ((bodys.find(uid) == bodys.end()) &&
            (prefetchedBodys.find(uid) == prefetchedBodys.end()) &&
            (requestedBodys.find(uid) == requestedBodys.end()))
        {
          if (m_PrefetchLevel >= PrefetchLevelCurrentMessage)
          {
            prefetchedBodys.insert(uid);
            prefetchBodyUidsPrimary.insert(uid);
          }
        }
      }
      else
      {
        if ((bodys.find(uid) == bodys.end()) &&
            (prefetchedBodys.find(uid) == prefetchedBodys.end()) &&
            (requestedBodys.find(uid) == requestedBodys.end()))
        {
          if (m_PrefetchLevel >= PrefetchLevelCurrentView)
          {
            prefetchedBodys.insert(uid);
            prefetchBodyUidsSecondary.insert(uid);
          }
        }
      }
    }
  }

  if (!prefetchBodyUidsPrimary.empty())
  {
    for (auto& uid : prefetchBodyUidsPrimary)
    {
      ImapManager::Request request;
      request.m_Folder = m_CurrentFolder;

      std::set<uint32_t> fetchUids;
      request.m_PrefetchLevel = PrefetchLevelCurrentMessage;
      fetchUids.insert(uid);
      request.m_GetBodys = fetchUids;

      LOG_DEBUG_VAR("request bodys =", fetchUids);
      m_ImapManager->PrefetchRequest(request);
    }
  }
    
  if (!prefetchBodyUidsSecondary.empty())
  {
    for (auto& uid : prefetchBodyUidsSecondary)
    {
      ImapManager::Request request;
      request.m_PrefetchLevel = PrefetchLevelCurrentView;
      request.m_Folder = m_CurrentFolder;

      std::set<uint32_t> fetchUids;
      fetchUids.insert(uid);
      request.m_GetBodys = fetchUids;

      LOG_DEBUG_VAR("request bodys =", fetchUids);
      m_ImapManager->PrefetchRequest(request);
    }
  }

  const int maxHeadersFetchRequest = 25;
  if (!fetchHeaderUids.empty())
  {
    std::set<uint32_t> subsetFetchHeaderUids;
    for (auto it = fetchHeaderUids.begin(); it != fetchHeaderUids.end(); ++it)
    {
      subsetFetchHeaderUids.insert(*it);
      if ((subsetFetchHeaderUids.size() == maxHeadersFetchRequest) ||
          (std::next(it) == fetchHeaderUids.end()))
      {
        ImapManager::Request request;
        request.m_Folder = m_CurrentFolder;
        request.m_GetHeaders = subsetFetchHeaderUids;

        LOG_DEBUG_VAR("request headers =", subsetFetchHeaderUids);
        m_ImapManager->AsyncRequest(request);
        
        subsetFetchHeaderUids.clear(); 
      }
    }
  }
  
  if (!fetchFlagUids.empty())
  {
    ImapManager::Request request;
    request.m_Folder = m_CurrentFolder;
    request.m_GetFlags = fetchFlagUids;
    
    LOG_DEBUG_VAR("request flags =", fetchFlagUids);
    m_ImapManager->AsyncRequest(request);
  }

  wrefresh(m_MainWin);
}

void Ui::DrawMessage()
{
  werase(m_MainWin);

  std::set<uint32_t> fetchBodyUids;
  bool markSeen = false;
  {
    std::lock_guard<std::mutex> lock(m_Mutex);
    std::map<uint32_t, Header>& headers = m_Headers[m_CurrentFolder];
    std::map<uint32_t, Body>& bodys = m_Bodys[m_CurrentFolder];

    std::set<uint32_t>& requestedBodys = m_RequestedBodys[m_CurrentFolder];

    int uid = m_MessageListCurrentUid[m_CurrentFolder];

    if ((uid != -1) &&
        (bodys.find(uid) == bodys.end()) &&
        (requestedBodys.find(uid) == requestedBodys.end()))
    {
      requestedBodys.insert(uid);
      fetchBodyUids.insert(uid);
    }

    std::string headerText;
    std::map<uint32_t, Header>::iterator headerIt = headers.find(uid);
    std::map<uint32_t, Body>::iterator bodyIt = bodys.find(uid);

    std::stringstream ss;
    if (headerIt != headers.end())
    {
      Header& header = headerIt->second;
      ss << "Date: " << header.GetDate() << "\n";
      ss << "From: " << header.GetFrom() << "\n";
      ss << "To: " << header.GetTo() << "\n";
      if (!header.GetCc().empty())
      {
        ss << "Cc: " << header.GetCc() << "\n";
      }
      
      ss << "Subject: " << header.GetSubject() << "\n";

      if (bodyIt != bodys.end())
      {
        Body& body = bodyIt->second;
        std::map<ssize_t, Part> parts = body.GetParts();
        std::vector<std::string> attnames;
        for (auto it = parts.begin(); it != parts.end(); ++it)
        {
          if (!it->second.m_Filename.empty())
          {
            attnames.push_back(it->second.m_Filename);
          }
        }

        if (!attnames.empty())
        {
          ss << "Attachments: ";
          ss << Util::Join(attnames, ", ");
          ss << "\n";
        }
      }

      ss << "\n";
    }

    headerText = ss.str();

    if (bodyIt != bodys.end())
    {
      Body& body = bodyIt->second;
      const std::string& bodyText = m_Plaintext ? body.GetTextPlain() : body.GetText();
      const std::string text = headerText + bodyText;
      const std::wstring wtext = Util::ToWString(text);
      const std::vector<std::wstring>& wlines = Util::WordWrap(wtext, m_ScreenWidth);
      int countLines = wlines.size();

      m_MessageViewLineOffset = Util::Bound(0, m_MessageViewLineOffset,
                                            countLines - m_MainWinHeight);
      for (int i = 0; ((i < m_MainWinHeight) && (i < countLines)); ++i)
      {
        const std::wstring& wdispStr = wlines.at(i + m_MessageViewLineOffset);
        const std::string& dispStr = Util::ToString(wdispStr);
        mvwprintw(m_MainWin, i, 0, "%s", dispStr.c_str());
      }

      markSeen = true;
    }
  }

  if (!fetchBodyUids.empty())
  {
    ImapManager::Request request;
    request.m_Folder = m_CurrentFolder;
    request.m_GetBodys = fetchBodyUids;
    LOG_DEBUG_VAR("request bodys =", fetchBodyUids);
    m_ImapManager->AsyncRequest(request);
  }

  if (markSeen)
  {
    MarkSeen();
  }
  
  wrefresh(m_MainWin);
}

void Ui::DrawComposeMessage()
{
  m_ComposeMessageLines = Util::WordWrap(m_ComposeMessageStr, m_ScreenWidth,
                                         m_ComposeMessagePos, m_ComposeMessageWrapLine,
                                         m_ComposeMessageWrapPos);

  int cursY = 0;
  int cursX = 0;
  if (m_IsComposeHeader)
  {
    if (m_ComposeHeaderLine < 4)
    {
      cursY = m_ComposeHeaderLine;
      cursX = m_ComposeHeaderPos + 10;
    }
  }
  else
  {
    cursY = 5 + m_ComposeMessageWrapLine;
    cursX = m_ComposeMessageWrapPos;
  }

  werase(m_MainWin);

  std::vector<std::wstring> composeLines;

  std::vector<std::wstring> headerLines =
  {
    L"To      : ",
    L"Cc      : ",
    L"Attchmnt: ",
    L"Subject : ",
  };

  for (int i = 0; i < (int)m_ComposeHeaderStr.size(); ++i)
  {
    if (i != m_ComposeHeaderLine)
    {
      std::wstring line = headerLines.at(i) + m_ComposeHeaderStr.at(i);
      composeLines.push_back(line.substr(0, m_ScreenWidth));
    }
    else
    {
      if (cursX >= m_ScreenWidth)
      {
        std::wstring line = headerLines.at(i) +
          m_ComposeHeaderStr.at(i).substr(cursX - m_ScreenWidth + 1);
        composeLines.push_back(line.substr(0, m_ScreenWidth));
        cursX = m_ScreenWidth - 1;
      }
      else
      {
        std::wstring line = headerLines.at(i) + m_ComposeHeaderStr.at(i);
        composeLines.push_back(line.substr(0, m_ScreenWidth));
      }
    }
  }

  composeLines.push_back(L"");

  for (auto line = m_ComposeMessageLines.begin(); line != m_ComposeMessageLines.end(); ++line)
  {
    composeLines.push_back(*line);
  }

  if (cursY < m_ComposeMessageOffsetY)
  {
    m_ComposeMessageOffsetY = std::max(m_ComposeMessageOffsetY - (m_MainWinHeight / 2), 0);

  }
  else if (cursY >= (m_ComposeMessageOffsetY + m_MainWinHeight))
  {
    m_ComposeMessageOffsetY += (m_MainWinHeight / 2);
  }

  int messageY = 0;
  int idx = 0;
  for (auto line = composeLines.begin(); line != composeLines.end(); ++line, ++idx)
  {
    if (idx < m_ComposeMessageOffsetY) continue;

    if (messageY > m_MainWinHeight) break;

    const std::string& dispStr = Util::ToString(*line);
    mvwprintw(m_MainWin, messageY, 0, "%s", dispStr.c_str());
    ++messageY;
  }

  cursY -= m_ComposeMessageOffsetY;

  leaveok(m_MainWin, false);
  wmove(m_MainWin, cursY, cursX);
  wrefresh(m_MainWin);
  leaveok(m_MainWin, true);
}

void Ui::DrawPartList()
{
  werase(m_MainWin);

  std::lock_guard<std::mutex> lock(m_Mutex);
  int uid = m_MessageListCurrentUid[m_CurrentFolder];
  std::map<uint32_t, Body>& bodys = m_Bodys[m_CurrentFolder];
  std::map<uint32_t, Body>::iterator bodyIt = bodys.find(uid);
  if (bodyIt != bodys.end())
  {
    Body& body = bodyIt->second;
    const std::map<ssize_t, Part>& parts = body.GetParts();

    int count = parts.size();
    if (count > 0)
    {
      m_PartListCurrentIndex = Util::Bound(0, m_PartListCurrentIndex, (int)parts.size() - 1);

      int itemsMax = m_MainWinHeight - 1;
      int idxOffs = Util::Bound(0, (int)(m_PartListCurrentIndex - ((itemsMax - 1) / 2)),
                                std::max(0, (int)parts.size() - (int)itemsMax));
      int idxMax = idxOffs + std::min(itemsMax, (int)parts.size());

      for (int i = idxOffs; i < idxMax; ++i)
      {
        auto it = std::next(parts.begin(), i);
        const Part& part = it->second;

        if (i == m_PartListCurrentIndex)
        {
          wattron(m_MainWin, A_REVERSE);
          m_PartListCurrentPart = part;
        }

        std::string leftPad = "    ";
        std::string sizeStr = std::to_string(part.m_Data.size()) + " bytes"; 
        std::string sizeStrPadded = Util::TrimPadString(sizeStr, 18);
        std::string mimeTypePadded = Util::TrimPadString(part.m_MimeType, 30);
        std::string line = leftPad + sizeStrPadded + mimeTypePadded;
        std::string filenamePadded =
          Util::TrimPadString(part.m_Filename, m_ScreenWidth - line.size());
        line = line + filenamePadded;

        std::wstring wline = Util::ToWString(line);
        mvwaddnwstr(m_MainWin, i - idxOffs, 0, wline.c_str(), wline.size());
        
        if (i == m_PartListCurrentIndex)
        {
          wattroff(m_MainWin, A_REVERSE);
        }
      }
    }
  }

  wrefresh(m_MainWin);
}

void Ui::AsyncUiRequest(char p_UiRequest)
{
  write(m_Pipe[1], &p_UiRequest, 1);
}

void Ui::PerformUiRequest(char p_UiRequest)
{
  if (p_UiRequest & UiRequestDrawAll)
  {
    DrawAll();
  }
}

void Ui::Run()
{
  DrawAll();
  m_Running = true;
  LOG_DEBUG("entering loop");

  while (m_Running)
  {
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    FD_SET(m_Pipe[0], &fds);
    int maxfd = std::max(STDIN_FILENO, m_Pipe[0]);
    struct timeval tv = {1, 0};
    int rv = select(maxfd + 1, &fds, NULL, NULL, &tv);

    if (rv == 0) continue;

    if (FD_ISSET(STDIN_FILENO, &fds))
    {
      wint_t key = 0;
      get_wch(&key);

      if (key == KEY_RESIZE)
      {
        CleanupWindows();
        InitWindows();
        DrawAll();
      }

      switch (m_State)
      {
        case StateViewMessageList:
          ViewMessageListKeyHandler(key);
          break;

        case StateViewMessage:
          ViewMessageKeyHandler(key);
          break;

        case StateGotoFolder:
        case StateMoveToFolder:
          ViewFolderListKeyHandler(key);
          break;

        case StateComposeMessage:
        case StateReplyMessage:
        case StateForwardMessage:
          ComposeMessageKeyHandler(key);
          break;

        case StateAddressList:
          ViewAddressListKeyHandler(key);
          break;

        case StateViewPartList:
          ViewPartListKeyHandler(key);
          break;

        default:
          break;
      }

      continue;
    }

    if (FD_ISSET(m_Pipe[0], &fds))
    {
      int len = 0;
      ioctl(m_Pipe[0], FIONREAD, &len);
      if (len > 0)
      {
        len = std::min(len, 256);
        std::vector<char> buf(len);
        read(m_Pipe[0], &buf[0], len);
        char uiRequest = UiRequestNone;
        for (int i = 0; i < len; ++i)
        {
          uiRequest |= buf[i];
        }

        PerformUiRequest(uiRequest);
      }
    }

  }

  LOG_DEBUG("exiting loop");
  
  return;
}

void Ui::ViewFolderListKeyHandler(int p_Key)
{
  if (p_Key == m_KeyCancel)
  {
    SetState(StateViewMessageList);
  }
  else if (p_Key == KEY_UP)
  {
    --m_FolderListCurrentIndex;
  }
  else if (p_Key == KEY_DOWN)
  {
    ++m_FolderListCurrentIndex;
  }
  else if (p_Key == KEY_PPAGE)
  {
    m_FolderListCurrentIndex = m_FolderListCurrentIndex - m_MainWinHeight;
  }
  else if (p_Key == KEY_NPAGE)
  {
    m_FolderListCurrentIndex = m_FolderListCurrentIndex + m_MainWinHeight;
  }
  else if (p_Key == KEY_RETURN)
  {
    if (m_State == StateGotoFolder)
    {
      m_CurrentFolder = m_FolderListCurrentFolder;
      m_ImapManager->SetCurrentFolder(m_CurrentFolder);
      SetState(StateViewMessageList);
      UpdateIndexFromUid();
    }
    else if (m_State == StateMoveToFolder)
    {
      if (m_FolderListCurrentFolder != m_CurrentFolder)
      {
        ImapManager::Action action;
        action.m_Folder = m_CurrentFolder;
        action.m_Uids.insert(m_MessageListCurrentUid[m_CurrentFolder]);
        action.m_MoveDestination = m_FolderListCurrentFolder;
        m_ImapManager->AsyncAction(action);

        m_HasRequestedUids[m_FolderListCurrentFolder] = false;

        {
          std::lock_guard<std::mutex> lock(m_Mutex);
          RemoveUidDate(m_CurrentFolder, action.m_Uids);
          m_Uids[m_CurrentFolder] = m_Uids[m_CurrentFolder] - action.m_Uids;
          m_Uids[m_FolderListCurrentFolder] = m_Uids[m_FolderListCurrentFolder] + action.m_Uids;
          m_Headers[m_CurrentFolder] = m_Headers[m_CurrentFolder] - action.m_Uids;
        }

        bool isMsgDateUidsEmpty = false;
        {
          std::lock_guard<std::mutex> lock(m_Mutex);
          isMsgDateUidsEmpty = m_MsgDateUids[m_CurrentFolder].empty();
        }

        UpdateUidFromIndex(true /* p_UserTriggered */);
        if (isMsgDateUidsEmpty)
        {
          SetState(StateViewMessageList);
        }
        else
        {
          SetState(m_LastState);
        }
      }
      else
      {
        SetDialogMessage("Move to same folder ignored");
        UpdateUidFromIndex(true /* p_UserTriggered */);
        SetState(m_LastState);
      }
    }
  }
  else if (p_Key == KEY_LEFT)
  {
    m_FolderListFilterPos = Util::Bound(0, m_FolderListFilterPos - 1,
                                        (int)m_FolderListFilterStr.size());
  }
  else if (p_Key == KEY_RIGHT)
  {
    m_FolderListFilterPos = Util::Bound(0, m_FolderListFilterPos + 1,
                                        (int)m_FolderListFilterStr.size());
  }
  else if (p_Key == KEY_SYS_BACKSPACE)
  {
    if (m_FolderListFilterPos > 0)
    {
      m_FolderListFilterStr.erase(--m_FolderListFilterPos, 1);
    }
  }
  else if (p_Key == KEY_DC)
  {
    if (m_FolderListFilterPos < (int)m_FolderListFilterStr.size())
    {
      m_FolderListFilterStr.erase(m_FolderListFilterPos, 1);
    }
  }
  else if (IsValidTextKey(p_Key))
  {
    m_FolderListFilterStr.insert(m_FolderListFilterPos++, 1, p_Key);
  }

  DrawAll();
}

void Ui::ViewAddressListKeyHandler(int p_Key)
{
  if (p_Key == m_KeyCancel)
  {
    SetState(m_LastMessageState);
  }
  else if (p_Key == KEY_UP)
  {
    --m_AddressListCurrentIndex;
  }
  else if (p_Key == KEY_DOWN)
  {
    ++m_AddressListCurrentIndex;
  }
  else if (p_Key == KEY_PPAGE)
  {
    m_AddressListCurrentIndex = m_AddressListCurrentIndex - m_MainWinHeight;
  }
  else if (p_Key == KEY_NPAGE)
  {
    m_AddressListCurrentIndex = m_AddressListCurrentIndex + m_MainWinHeight;
  }
  else if (p_Key == KEY_RETURN)
  {
    std::wstring address;
    const std::string& oldAddress =
      Util::Trim(Util::ToString(m_ComposeHeaderStr[m_ComposeHeaderLine].substr(0, m_ComposeHeaderPos)));
    if (!oldAddress.empty() && (oldAddress[oldAddress.size() - 1] != ','))
    {
      address = Util::ToWString(", " + m_AddressListCurrentAddress);
    }
    else
    {
      address = Util::ToWString(m_AddressListCurrentAddress);
    }

    m_ComposeHeaderStr[m_ComposeHeaderLine].insert(m_ComposeHeaderPos, address);
    m_ComposeHeaderPos += address.size();
    SetState(m_LastMessageState);
  }
  else if (p_Key == KEY_LEFT)
  {
    m_AddressListFilterPos = Util::Bound(0, m_AddressListFilterPos - 1,
                                         (int)m_AddressListFilterStr.size());
  }
  else if (p_Key == KEY_RIGHT)
  {
    m_AddressListFilterPos = Util::Bound(0, m_AddressListFilterPos + 1,
                                         (int)m_AddressListFilterStr.size());
  }
  else if (p_Key == KEY_SYS_BACKSPACE)
  {
    if (m_AddressListFilterPos > 0)
    {
      m_AddressListFilterStr.erase(--m_AddressListFilterPos, 1);
    }
  }
  else if (p_Key == KEY_DC)
  {
    if (m_AddressListFilterPos < (int)m_AddressListFilterStr.size())
    {
      m_AddressListFilterStr.erase(m_AddressListFilterPos, 1);
    }
  }
  else if (IsValidTextKey(p_Key))
  {
    m_AddressListFilterStr.insert(m_AddressListFilterPos++, 1, p_Key);
  }

  DrawAll();
}

void Ui::ViewMessageListKeyHandler(int p_Key)
{
  if (p_Key == m_KeyQuit)
  {
    m_Running = false;
    LOG_DEBUG("stop thread");
  }
  else if (p_Key == m_KeyRefresh)
  {
    if (IsConnected())
    {
      InvalidateUiCache(m_CurrentFolder);
    }
    else
    {
      SetDialogMessage("Cannot refresh while offline");
    }
  }
  else if ((p_Key == KEY_UP) || (p_Key == m_KeyPrevMsg))
  {
    --m_MessageListCurrentIndex[m_CurrentFolder];
    UpdateUidFromIndex(true /* p_UserTriggered */);
  }
  else if ((p_Key == KEY_DOWN) || (p_Key == m_KeyNextMsg))
  {
    ++m_MessageListCurrentIndex[m_CurrentFolder];
    UpdateUidFromIndex(true /* p_UserTriggered */);
  }
  else if (p_Key == KEY_PPAGE)
  {
    m_MessageListCurrentIndex[m_CurrentFolder] =
      m_MessageListCurrentIndex[m_CurrentFolder] - m_MainWinHeight;
    UpdateUidFromIndex(true /* p_UserTriggered */);
  }
  else if ((p_Key == KEY_NPAGE) || (p_Key == KEY_SPACE))
  {
    m_MessageListCurrentIndex[m_CurrentFolder] =
      m_MessageListCurrentIndex[m_CurrentFolder] + m_MainWinHeight;
    UpdateUidFromIndex(true /* p_UserTriggered */);
  }
  else if ((p_Key == KEY_RETURN) || (p_Key == m_KeyOpen))
  {
    if (m_MessageListCurrentUid[m_CurrentFolder] != -1)
    {
      SetState(StateViewMessage);
    }
  }
  else if ((p_Key == m_KeyGotoFolder) || (p_Key == m_KeyBack))
  {
    SetState(StateGotoFolder);
  }
  else if (p_Key == m_KeyMove)
  {
    if (IsConnected())
    {
      if (m_MessageListCurrentUid[m_CurrentFolder] != -1)
      {
        SetState(StateMoveToFolder);
      }
      else
      {
        SetDialogMessage("No message to move");
      }
    }
    else
    {
      SetDialogMessage("Cannot move while offline");
    }
  }
  else if (p_Key == m_KeyCompose)
  {
    if (IsConnected())
    {
      SetState(StateComposeMessage);
    }
    else
    {
      SetDialogMessage("Cannot compose while offline");
    }
  }
  else if (p_Key == m_KeyReply)
  {
    if (IsConnected())
    {
      if (m_MessageListCurrentUid[m_CurrentFolder] != -1)
      {
        if (CurrentMessageBodyAvailable())
        {
          SetState(StateReplyMessage);
        }
        else
        {
          SetDialogMessage("Cannot reply message not fetched");
        }
      }
      else
      {
        SetDialogMessage("No message to reply");
      }
    }
    else
    {
      SetDialogMessage("Cannot reply while offline");
    }
  }
  else if (p_Key == m_KeyForward)
  {
    if (IsConnected())
    {
      if (m_MessageListCurrentUid[m_CurrentFolder] != -1)
      {
        if (CurrentMessageBodyAvailable())
        {
          SetState(StateForwardMessage);
        }
        else
        {
          SetDialogMessage("Cannot forward message not fetched");
        }
      }
      else
      {
        SetDialogMessage("No message to forward");
      }
    }
    else
    {
      SetDialogMessage("Cannot forward while offline");
    }
  }
  else if ((p_Key == m_KeyDelete) || (p_Key == KEY_DC))
  {
    if (IsConnected())
    {
      if (m_MessageListCurrentUid[m_CurrentFolder] != -1)
      {
        DeleteMessage();
      }
      else
      {
        SetDialogMessage("No message to delete");
      }
    }
    else
    {
      SetDialogMessage("Cannot delete while offline");
    }
  }
  else if (p_Key == m_KeyToggleUnread)
  {
    if (IsConnected())
    {
      if (m_MessageListCurrentUid[m_CurrentFolder] != -1)
      {
        ToggleUnseen();
      }
      else
      {
        SetDialogMessage("No message to toggle read/unread");
      }
    }
    else
    {
      SetDialogMessage("Cannot toggle read/unread while offline");
    }
  }
  else
  {
    SetDialogMessage("Invalid input (" + Util::ToHexString(p_Key) +  ")");
  }

  DrawAll();
}

void Ui::ViewMessageKeyHandler(int p_Key)
{
  if (p_Key == m_KeyQuit)
  {
    m_Running = false;
    LOG_DEBUG("stop thread");
  }
  else if (p_Key == KEY_UP)
  {
    --m_MessageViewLineOffset;
  }
  else if (p_Key == KEY_DOWN)
  {
    ++m_MessageViewLineOffset;
  }
  else if (p_Key == m_KeyPrevMsg)
  {
    --m_MessageListCurrentIndex[m_CurrentFolder];
    m_MessageViewLineOffset = 0;
    UpdateUidFromIndex(true /* p_UserTriggered */);
  }
  else if (p_Key == m_KeyNextMsg)
  {
    ++m_MessageListCurrentIndex[m_CurrentFolder];
    m_MessageViewLineOffset = 0;
    UpdateUidFromIndex(true /* p_UserTriggered */);
  }
  else if (p_Key == KEY_PPAGE)
  {
    m_MessageViewLineOffset = m_MessageViewLineOffset - m_MainWinHeight;
  }
  else if ((p_Key == KEY_NPAGE) || (p_Key == KEY_SPACE))
  {
    m_MessageViewLineOffset = m_MessageViewLineOffset + m_MainWinHeight;
  }
  else if ((p_Key == KEY_SYS_BACKSPACE) || (p_Key == m_KeyBack))
  {
    SetState(StateViewMessageList);
  }
  else if (p_Key == m_KeyOpen)
  {
    SetState(StateViewPartList);
  }
  else if (p_Key == m_KeyGotoFolder)
  {
    SetState(StateGotoFolder);
  }
  else if (p_Key == m_KeyMove)
  {
    if (IsConnected())
    {
      SetState(StateMoveToFolder);
    }
    else
    {
      SetDialogMessage("Cannot move while offline");
    }
  }
  else if (p_Key == m_KeyCompose)
  {
    if (IsConnected())
    {
      SetState(StateComposeMessage);
    }
    else
    {
      SetDialogMessage("Cannot compose while offline");
    }
  }
  else if (p_Key == m_KeyReply)
  {
    if (IsConnected())
    {
      if (CurrentMessageBodyAvailable())
      {
        SetState(StateReplyMessage);
      }
      else
      {
        SetDialogMessage("Cannot reply message not fetched");
      }
    }
    else
    {
      SetDialogMessage("Cannot reply while offline");
    }
  }
  else if (p_Key == m_KeyForward)
  {
    if (IsConnected())
    {
      if (CurrentMessageBodyAvailable())
      {
        SetState(StateForwardMessage);
      }
      else
      {
        SetDialogMessage("Cannot forward message not fetched");
      }
    }
    else
    {
      SetDialogMessage("Cannot forward while offline");
    }
  }
  else if (p_Key == m_KeyToggleTextHtml)
  {
    m_Plaintext = !m_Plaintext;
  }
  else if ((p_Key == m_KeyDelete) || (p_Key == KEY_DC))
  {
    if (IsConnected())
    {
      DeleteMessage();
    }
    else
    {
      SetDialogMessage("Cannot delete while offline");
    }
  }
  else if (p_Key == m_KeyToggleUnread)
  {
    if (IsConnected())
    {
      ToggleUnseen();
    }
    else
    {
      SetDialogMessage("Cannot toggle read/unread while offline");
    }
  }
  else
  {
    SetDialogMessage("Invalid input (" + Util::ToHexString(p_Key) +  ")");
  }

  DrawAll();
}

void Ui::ComposeMessageKeyHandler(int p_Key)
{
  if (m_IsComposeHeader)
  {
    if (p_Key == m_KeyCancel)
    {
      if (Ui::PromptConfirmCancelCompose())
      {
        SetState(m_LastState);
      }
    }
    else if (p_Key == m_KeySend)
    {
      SendComposedMessage();
      SetState(m_LastState);
    }
    else if (p_Key == m_KeyAddressBook)
    {
      if (m_ComposeHeaderLine < 2)
      {
        SetState(StateAddressList);
      }
    }
    else if (p_Key == KEY_UP)
    {
      m_ComposeHeaderLine = Util::Bound(0, m_ComposeHeaderLine - 1,
                                        (int)m_ComposeHeaderStr.size() - 1);
      m_ComposeHeaderPos = Util::Bound(0, m_ComposeHeaderPos,
                                       (int)m_ComposeHeaderStr.at(m_ComposeHeaderLine).size());
    }
    else if ((p_Key == KEY_DOWN) || (p_Key == KEY_RETURN) || (p_Key == KEY_TAB))
    {
      if (m_ComposeHeaderLine < ((int)m_ComposeHeaderStr.size() - 1))
      {
        m_ComposeHeaderLine = Util::Bound(0, m_ComposeHeaderLine + 1,
                                          (int)m_ComposeHeaderStr.size() - 1);
        m_ComposeHeaderPos = Util::Bound(0, m_ComposeHeaderPos,
                                         (int)m_ComposeHeaderStr.at(m_ComposeHeaderLine).size());
      }
      else
      {
        m_IsComposeHeader = false;
      }
    }
    else if (p_Key == KEY_PPAGE)
    {
      m_ComposeHeaderLine = 0;
      m_ComposeHeaderPos = 0;
    }
    else if (p_Key == KEY_NPAGE)
    {
      m_IsComposeHeader = false;
    }
    else if (p_Key == KEY_LEFT)
    {
      m_ComposeHeaderPos = Util::Bound(0, m_ComposeHeaderPos - 1,
                                       (int)m_ComposeHeaderStr.at(m_ComposeHeaderLine).size());
    }
    else if (p_Key == KEY_RIGHT)
    {
      m_ComposeHeaderPos = Util::Bound(0, m_ComposeHeaderPos + 1,
                                       (int)m_ComposeHeaderStr.at(m_ComposeHeaderLine).size());
    }    
    else if (p_Key == KEY_SYS_BACKSPACE)
    {
      if (m_ComposeHeaderPos > 0)
      {
        m_ComposeHeaderStr[m_ComposeHeaderLine].erase(--m_ComposeHeaderPos, 1);
      }
    }
    else if (p_Key == KEY_DC)
    {
      if (m_ComposeHeaderPos < (int)m_ComposeHeaderStr[m_ComposeHeaderLine].size())
      {
        m_ComposeHeaderStr[m_ComposeHeaderLine].erase(m_ComposeHeaderPos, 1);
      }
    }
    else if (p_Key == m_KeyDeleteLine)
    {
      Util::DeleteToMatch(m_ComposeHeaderStr[m_ComposeHeaderLine], m_ComposeHeaderPos, '\n');
    }
    else if (IsValidTextKey(p_Key))
    {
      m_ComposeHeaderStr[m_ComposeHeaderLine].insert(m_ComposeHeaderPos++, 1, p_Key);
    }
    else
    {
      SetDialogMessage("Invalid input (" + Util::ToHexString(p_Key) +  ")");
    }
  }
  else
  {
    if (p_Key == m_KeyCancel)
    {
      if (Ui::PromptConfirmCancelCompose())
      {
        SetState(m_LastState);
      }
    }
    else if (p_Key == m_KeySend)
    {
      SendComposedMessage();
      SetState(m_LastState);
    }
    else if (p_Key == KEY_UP)
    {
      ComposeMessagePrevLine();
    }
    else if (p_Key == KEY_DOWN)
    {
      ComposeMessageNextLine();
    }
    else if (p_Key == KEY_PPAGE)
    {
      for (int i = 0; i < (m_MainWinHeight / 2); ++i)
      {
        ComposeMessagePrevLine();
        m_ComposeMessageLines = Util::WordWrap(m_ComposeMessageStr, m_ScreenWidth,
                                               m_ComposeMessagePos, m_ComposeMessageWrapLine,
                                               m_ComposeMessageWrapPos);
      }
    }
    else if (p_Key == KEY_NPAGE)
    {
      for (int i = 0; i < (m_MainWinHeight / 2); ++i)
      {
        ComposeMessageNextLine();
        m_ComposeMessageLines = Util::WordWrap(m_ComposeMessageStr, m_ScreenWidth,
                                               m_ComposeMessagePos, m_ComposeMessageWrapLine,
                                               m_ComposeMessageWrapPos);
      }
    }
    else if (p_Key == KEY_LEFT)
    {
      m_ComposeMessagePos = Util::Bound(0, m_ComposeMessagePos - 1,
                                        (int)m_ComposeMessageStr.size());
    }
    else if (p_Key == KEY_RIGHT)
    {
      m_ComposeMessagePos = Util::Bound(0, m_ComposeMessagePos + 1,
                                        (int)m_ComposeMessageStr.size());
    }
    else if (p_Key == KEY_SYS_BACKSPACE)
    {
      if (m_ComposeMessagePos > 0)
      {
        m_ComposeMessageStr.erase(--m_ComposeMessagePos, 1);
      }
    }
    else if (p_Key == KEY_DC)
    {
      if (m_ComposeMessagePos < (int)m_ComposeMessageStr.size())
      {
        m_ComposeMessageStr.erase(m_ComposeMessagePos, 1);
      }
    }
    else if (p_Key == m_KeyDeleteLine)
    {
      Util::DeleteToMatch(m_ComposeMessageStr, m_ComposeMessagePos, '\n');
    }
    else if (IsValidTextKey(p_Key))
    {
      m_ComposeMessageStr.insert(m_ComposeMessagePos++, 1, p_Key);
    }
    else
    {
      SetDialogMessage("Invalid input (" + Util::ToHexString(p_Key) +  ")");
    }
  }

  DrawAll();
}

void Ui::ViewPartListKeyHandler(int p_Key)
{
  if (p_Key == m_KeyQuit)
  {
    m_Running = false;
    LOG_DEBUG("stop thread");
  }
  else if ((p_Key == KEY_UP) || (p_Key == m_KeyPrevMsg))
  {
    --m_PartListCurrentIndex;
  }
  else if ((p_Key == KEY_DOWN) || (p_Key == m_KeyNextMsg))
  {
    ++m_PartListCurrentIndex;
  }
  else if (p_Key == KEY_PPAGE)
  {
    m_PartListCurrentIndex = m_PartListCurrentIndex - m_MainWinHeight;
  }
  else if ((p_Key == KEY_NPAGE) || (p_Key == KEY_SPACE))
  {
    m_PartListCurrentIndex = m_MessageListCurrentIndex[m_CurrentFolder] + m_MainWinHeight;
  }
  else if ((p_Key == KEY_SYS_BACKSPACE) || (p_Key == m_KeyBack))
  {
    SetState(StateViewMessage);
  }
  else if ((p_Key == KEY_RETURN) || (p_Key == m_KeyOpen))
  {
    std::string ext;
    std::string err;
    if (!m_PartListCurrentPart.m_Filename.empty())
    {
      ext = Util::GetFileExt(m_PartListCurrentPart.m_Filename);
      err = "Cannot determine file extension for " + m_PartListCurrentPart.m_Filename;
    }
    else
    {
      ext = Util::ExtensionForMimeType(m_PartListCurrentPart.m_MimeType);
      err = "Unknown MIME type " + m_PartListCurrentPart.m_MimeType;
    }

    if (!ext.empty())
    {
      const std::string& tempFilename = Util::GetTempFilename(ext);
      Util::WriteFile(tempFilename, m_PartListCurrentPart.m_Data);
      SetDialogMessage("Waiting for external viewer to exit");
      DrawDialog();
      Util::OpenInExtViewer(tempFilename);
      Util::DeleteFile(tempFilename);
      SetDialogMessage("");
    }
    else
    {
      SetDialogMessage(err);
    }
  }
  else if (p_Key == m_KeySaveFile)
  {
    std::string filename = m_PartListCurrentPart.m_Filename;
    if (PromptString("Save Filename: ", filename))
    {
      if (!filename.empty())
      {
        Util::WriteFile(filename, m_PartListCurrentPart.m_Data);
        SetDialogMessage("File saved");
      }
      else
      {
        SetDialogMessage("Save cancelled (empty filename)");
      }
    }
    else
    {
      SetDialogMessage("Save cancelled");
    }
  }
  else
  {
    SetDialogMessage("Invalid input (" + Util::ToHexString(p_Key) +  ")");
  }

  DrawAll();
}

void Ui::SetState(Ui::State p_State)
{
  if (p_State == StateAddressList)
  {
    // going to address list
    m_LastMessageState = m_State;
    m_State = p_State;
  }
  else if (m_State != StateAddressList)
  {
    // normal state transition
    m_LastState = m_State;
    m_State = p_State;
  }
  else
  {
    // exiting address list
    m_State = p_State;
    return;
  }
  
  if (m_State == StateGotoFolder)
  {
    curs_set(1);
    m_FolderListFilterPos = 0;
    m_FolderListFilterStr.clear();
    m_FolderListCurrentFolder = m_CurrentFolder;
    m_FolderListCurrentIndex = INT_MAX;
  }
  else if (m_State == StateMoveToFolder)
  {
    curs_set(1);
    if (!m_PersistFolderFilter)
    {
      m_FolderListFilterPos = 0;
      m_FolderListFilterStr.clear();
      m_FolderListCurrentFolder = m_CurrentFolder;
      m_FolderListCurrentIndex = INT_MAX;
    }
  }
  else if (m_State == StateViewMessageList)
  {
    curs_set(0);
  }
  else if (m_State == StateViewMessage)
  {
    curs_set(0);
    m_MessageViewLineOffset = 0;
  }
  else if (m_State == StateComposeMessage)
  {
    curs_set(1);
    m_ComposeHeaderStr.clear();
    m_ComposeHeaderStr[0] = L"";
    m_ComposeHeaderStr[1] = L"";
    m_ComposeHeaderStr[2] = L"";
    m_ComposeHeaderStr[3] = L"";
    m_ComposeHeaderLine = 0;
    m_ComposeHeaderPos = 0;
    m_ComposeHeaderRef.clear();
    m_ComposeMessageStr.clear();
    m_ComposeMessagePos = 0;
    m_IsComposeHeader = true;
  }
  else if (m_State == StateReplyMessage)
  {
    curs_set(1);
    m_ComposeHeaderStr.clear();
    m_ComposeHeaderStr[0] = L"";
    m_ComposeHeaderStr[1] = L"";
    m_ComposeHeaderStr[2] = L"";
    m_ComposeHeaderStr[3] = L"";
    m_ComposeHeaderLine = 3;
    m_ComposeHeaderPos = 0;
    m_ComposeMessageStr.clear();
    m_ComposeMessagePos = 0;

    std::lock_guard<std::mutex> lock(m_Mutex);
    std::map<uint32_t, Header>& headers = m_Headers[m_CurrentFolder];
    std::map<uint32_t, Body>& bodys = m_Bodys[m_CurrentFolder];

    std::map<uint32_t, Header>::iterator hit = headers.find(m_MessageListCurrentUid[m_CurrentFolder]);
    std::map<uint32_t, Body>::iterator bit = bodys.find(m_MessageListCurrentUid[m_CurrentFolder]);
    if ((hit != headers.end()) && (bit != bodys.end()))
    {
      Header& header = hit->second;
      Body& body = bit->second;

      const std::string& bodyText = m_Plaintext ? body.GetTextPlain() : body.GetText();
      m_ComposeMessageStr = Util::ToWString("\n\nOn " + header.GetDate() + " " +
                                            header.GetFrom() +
                                            " wrote:\n\n" +
                                            Util::AddIndent(bodyText, "> "));

      // @todo: handle quoted commas in address name
      std::vector<std::string> ccs = Util::Split(header.GetCc(), ',');
      std::vector<std::string> tos = Util::Split(header.GetTo(), ',');
      ccs.insert(ccs.end(), tos.begin(), tos.end());
      std::string selfAddress = m_SmtpManager->GetAddress();
      for (auto it = ccs.begin(); it != ccs.end(); /* incremented in loop */)
      {
        it = ((it->find(selfAddress) == std::string::npos) &&
              (it->find(header.GetFrom()) == std::string::npos)) ? std::next(it) : ccs.erase(it);
      }

      m_ComposeHeaderStr[0] = Util::ToWString(header.GetFrom());
      m_ComposeHeaderStr[1] = Util::ToWString(Util::Join(ccs, ", "));
      m_ComposeHeaderStr[2] = L"";
      m_ComposeHeaderStr[3] = Util::ToWString(Util::MakeReplySubject(header.GetSubject()));

      m_ComposeHeaderRef = header.GetMessageId();
    }

    m_IsComposeHeader = false;
  }
  else if (m_State == StateForwardMessage)
  {
    curs_set(1);
    m_ComposeHeaderStr.clear();
    m_ComposeHeaderStr[0] = L"";
    m_ComposeHeaderStr[1] = L"";
    m_ComposeHeaderStr[2] = L"";
    m_ComposeHeaderStr[3] = L"";
    m_ComposeHeaderLine = 0;
    m_ComposeHeaderPos = 0;
    m_ComposeMessageStr.clear();
    m_ComposeMessagePos = 0;

    std::lock_guard<std::mutex> lock(m_Mutex);
    std::map<uint32_t, Header>& headers = m_Headers[m_CurrentFolder];
    std::map<uint32_t, Body>& bodys = m_Bodys[m_CurrentFolder];

    std::map<uint32_t, Header>::iterator hit = headers.find(m_MessageListCurrentUid[m_CurrentFolder]);
    std::map<uint32_t, Body>::iterator bit = bodys.find(m_MessageListCurrentUid[m_CurrentFolder]);
    if ((hit != headers.end()) && (bit != bodys.end()))
    {
      Header& header = hit->second;
      Body& body = bit->second;

      int idx = 0;
      std::string tmppath = Util::GetTempDir() + "forward/";
      Util::MkDir(tmppath);
      for (auto& part : body.GetParts())
      {
        if (!part.second.m_Filename.empty())
        {
          std::string tmpfiledir = tmppath + std::to_string(idx++) + "/";
          Util::MkDir(tmpfiledir);
          std::string tmpfilepath = tmpfiledir + part.second.m_Filename;

          Util::WriteFile(tmpfilepath, part.second.m_Data);
          if (m_ComposeHeaderStr[2].empty())
          {
            m_ComposeHeaderStr[2] = m_ComposeHeaderStr[2] + Util::ToWString(tmpfilepath);
          }
          else
          {
            m_ComposeHeaderStr[2] = m_ComposeHeaderStr[2] + L", " + Util::ToWString(tmpfilepath);
          }
        }
      }

      m_ComposeMessageStr =
        Util::ToWString("\n\n---------- Forwarded message ---------\n"
                        "From: " + header.GetFrom() + "\n"
                        "Date: " + header.GetDate() + "\n"
                        "Subject: " + header.GetSubject() + "\n"
                        "To: " + header.GetTo() + "\n");
      if (!header.GetCc().empty())
      {
        m_ComposeMessageStr +=
          Util::ToWString("Cc: " + header.GetCc());
      }

      const std::string& bodyText = m_Plaintext ? body.GetTextPlain() : body.GetText();
      m_ComposeMessageStr += Util::ToWString("\n" + bodyText + "\n");
      m_ComposeHeaderStr[3] = Util::ToWString(Util::MakeForwardSubject(header.GetSubject()));

      m_ComposeHeaderRef = header.GetMessageId();
    }

    m_IsComposeHeader = true;
  }
  else if (m_State == StateAddressList)
  {
    curs_set(1);
    m_AddressListFilterPos = 0;
    m_AddressListFilterStr.clear();
    m_Addresses = AddressBook::Get();
    m_AddressListCurrentIndex = 0;
    m_AddressListCurrentAddress = "";
  }
  else if (m_State == StateViewPartList)
  {
    curs_set(0);
    m_PartListCurrentIndex = 0;
  }
}

void Ui::ResponseHandler(const ImapManager::Request& p_Request, const ImapManager::Response& p_Response)
{
  char uiRequest = UiRequestNone;

  bool updateIndexFromUid = false;
  
  if (p_Request.m_PrefetchLevel < PrefetchLevelFullSync)
  {
    if (p_Request.m_GetFolders && !(p_Response.m_ResponseStatus & ImapManager::ResponseStatusGetFoldersFailed))
    {
      std::lock_guard<std::mutex> lock(m_Mutex);
      m_Folders = p_Response.m_Folders;
      uiRequest |= UiRequestDrawAll;
      LOG_DEBUG_VAR("new folders =", p_Response.m_Folders);
    }

    if (p_Request.m_GetUids && !(p_Response.m_ResponseStatus & ImapManager::ResponseStatusGetUidsFailed))
    {
      std::lock_guard<std::mutex> lock(m_Mutex);
      if (m_Uids[p_Response.m_Folder].empty())
      {
        m_NewUids[p_Response.m_Folder] = p_Response.m_Uids;
      }
      else
      {
        const std::set<uint32_t>& uids = m_Uids[p_Response.m_Folder];
        std::set<uint32_t>& newUids = m_NewUids[p_Response.m_Folder];
        for (auto& uid : p_Response.m_Uids)
        {
          if (uids.find(uid) == uids.end())
          {
            newUids.insert(uid);
          }
        }
      }

      const std::set<uint32_t>& removedUids = m_Uids[p_Response.m_Folder] - p_Response.m_Uids;
      if (!removedUids.empty())
      {
        LOG_DEBUG_VAR("del uids =", removedUids);
        RemoveUidDate(p_Response.m_Folder, removedUids);
      }
      
      m_Uids[p_Response.m_Folder] = p_Response.m_Uids;
      uiRequest |= UiRequestDrawAll;
      updateIndexFromUid = true;
      LOG_DEBUG_VAR("new uids =", p_Response.m_Uids);
    }

    if (!p_Request.m_GetHeaders.empty() && !(p_Response.m_ResponseStatus & ImapManager::ResponseStatusGetHeadersFailed))
    {
      std::lock_guard<std::mutex> lock(m_Mutex);
      m_Headers[p_Response.m_Folder].insert(p_Response.m_Headers.begin(), p_Response.m_Headers.end());
      uiRequest |= UiRequestDrawAll;

      AddUidDate(p_Response.m_Folder, p_Response.m_Headers);

      for (auto& header : p_Response.m_Headers)
      {
        AddressBook::Add(m_Headers[p_Response.m_Folder][header.first].GetUniqueId(),
                         m_Headers[p_Response.m_Folder][header.first].GetAddresses());
      }

      updateIndexFromUid = true;
      LOG_DEBUG_VAR("new headers =", MapKey(p_Response.m_Headers));
    }

    if (!p_Request.m_GetFlags.empty() && !(p_Response.m_ResponseStatus & ImapManager::ResponseStatusGetFlagsFailed))
    {
      std::lock_guard<std::mutex> lock(m_Mutex);
      std::map<uint32_t, uint32_t> newFlags = p_Response.m_Flags;
      newFlags.insert(m_Flags[p_Response.m_Folder].begin(), m_Flags[p_Response.m_Folder].end());
      m_Flags[p_Response.m_Folder] = newFlags;
      uiRequest |= UiRequestDrawAll;
      LOG_DEBUG_VAR("new flags =", MapKey(p_Response.m_Flags));
    }

    if (!p_Request.m_GetBodys.empty() && !(p_Response.m_ResponseStatus & ImapManager::ResponseStatusGetBodysFailed))
    {
      std::lock_guard<std::mutex> lock(m_Mutex);
      m_Bodys[p_Response.m_Folder].insert(p_Response.m_Bodys.begin(), p_Response.m_Bodys.end());
      uiRequest |= UiRequestDrawAll;
      LOG_DEBUG_VAR("new bodys =", MapKey(p_Response.m_Bodys));
    }
  }
  
  if (m_PrefetchLevel == PrefetchLevelFullSync)
  {
    if (p_Request.m_GetFolders && !(p_Response.m_ResponseStatus & ImapManager::ResponseStatusGetFoldersFailed))
    {
      std::lock_guard<std::mutex> lock(m_Mutex);
      for (auto& folder : p_Response.m_Folders)
      {
        if (!m_HasRequestedUids[folder] && !m_HasPrefetchRequestedUids[folder])
        {
          ImapManager::Request request;
          request.m_PrefetchLevel = PrefetchLevelFullSync;
          request.m_Folder = folder;
          request.m_GetUids = true;
          LOG_DEBUG_VAR("request uids =", folder);
          m_HasPrefetchRequestedUids[folder] = true;
          m_ImapManager->PrefetchRequest(request);
        }
      }
    }

    if (p_Request.m_GetUids && !(p_Response.m_ResponseStatus & ImapManager::ResponseStatusGetUidsFailed))
    {
      std::lock_guard<std::mutex> lock(m_Mutex);
      const int maxMessagesFetchRequest = 5;
      const std::set<uint32_t>& fetchHeaderUids = p_Response.m_Uids;
      if (!fetchHeaderUids.empty())
      {
        std::set<uint32_t> subsetFetchHeaderUids;
        for (auto it = fetchHeaderUids.begin(); it != fetchHeaderUids.end(); ++it)
        {
          subsetFetchHeaderUids.insert(*it);
          if ((subsetFetchHeaderUids.size() == maxMessagesFetchRequest) ||
              (std::next(it) == fetchHeaderUids.end()))
          {
            ImapManager::Request request;
            request.m_PrefetchLevel = PrefetchLevelFullSync;
            request.m_Folder = p_Response.m_Folder;;
            request.m_GetHeaders = subsetFetchHeaderUids;
            request.m_GetBodys = subsetFetchHeaderUids;

            LOG_DEBUG_VAR("request headers =", subsetFetchHeaderUids);
            LOG_DEBUG_VAR("request bodys =", subsetFetchHeaderUids);
            m_ImapManager->PrefetchRequest(request);
        
            subsetFetchHeaderUids.clear(); 
          }
        }
      }      
    }
  }
  
  if (p_Response.m_ResponseStatus != ImapManager::ResponseStatusOk)
  {
    if (p_Response.m_ResponseStatus & ImapManager::ResponseStatusGetFoldersFailed)
    {
      SetDialogMessage("Get folders failed");
    }
    else if (p_Response.m_ResponseStatus & ImapManager::ResponseStatusGetBodysFailed)
    {
      SetDialogMessage("Get message body failed");
    }
    else if (p_Response.m_ResponseStatus & ImapManager::ResponseStatusGetHeadersFailed)
    {
      SetDialogMessage("Get message headers failed");
    }
    else if (p_Response.m_ResponseStatus & ImapManager::ResponseStatusGetUidsFailed)
    {
      SetDialogMessage("Get message ids failed");
    }
    else if (p_Response.m_ResponseStatus & ImapManager::ResponseStatusGetFlagsFailed)
    {
      SetDialogMessage("Get message flags failed");
    }
  }

  if (updateIndexFromUid)
  {
    UpdateIndexFromUid();
  }
  
  AsyncUiRequest(uiRequest);
}

void Ui::ResultHandler(const ImapManager::Action& p_Action, const ImapManager::Result& p_Result)
{
  if (!p_Result.m_Result)
  {
    if (!p_Action.m_MoveDestination.empty())
    {
      SetDialogMessage("Move message failed");
    }
    else if (p_Action.m_SetSeen || p_Action.m_SetUnseen)
    {
      SetDialogMessage("Update message flags failed");
    }
  }
}

void Ui::SmtpResultHandler(const SmtpManager::Result& p_Result)
{
  if (!p_Result.m_Result)
  {
    SetDialogMessage("Send message failed");
  }
  else
  {
    const SmtpManager::Action& action = p_Result.m_Action;
    const std::vector<Contact> to = Contact::FromStrings(Util::Trim(Util::Split(action.m_To)));
    const std::vector<Contact> cc = Contact::FromStrings(Util::Trim(Util::Split(action.m_Cc)));

    std::vector<Contact> contacts;
    std::move(to.begin(), to.end(), std::back_inserter(contacts));
    std::move(cc.begin(), cc.end(), std::back_inserter(contacts));

    for (auto& contact : contacts)
    {
      const std::string& address = contact.GetAddress();

      if (address == m_Address)
      {
        sleep(1);
        InvalidateUiCache(m_Inbox);
        AsyncUiRequest(UiRequestDrawAll);
        break;
      }
    }
  }

  std::string tmppath = Util::GetTempDir() + "forward/";
  Util::RmDir(tmppath); 
}

void Ui::StatusHandler(const StatusUpdate& p_StatusUpdate)
{
  std::lock_guard<std::mutex> lock(m_Mutex);
  m_Status.Update(p_StatusUpdate);

  if (!m_HasRequestedFolders && !m_HasPrefetchRequestedFolders && (m_PrefetchLevel >= PrefetchLevelFullSync) &&
      (p_StatusUpdate.SetFlags & Status::FlagConnected))
  {
    ImapManager::Request request;
    request.m_PrefetchLevel = PrefetchLevelFullSync;
    request.m_GetFolders = true;
    LOG_DEBUG("request folders");
    m_HasPrefetchRequestedFolders = true;
    m_ImapManager->PrefetchRequest(request);
  }
  
  AsyncUiRequest(UiRequestDrawAll);
}

void Ui::SetImapManager(std::shared_ptr<ImapManager> p_ImapManager)
{
  m_ImapManager = p_ImapManager;
  if (m_ImapManager)
  {
    m_ImapManager->SetCurrentFolder(m_CurrentFolder);
  }
}

void Ui::SetSmtpManager(std::shared_ptr<SmtpManager> p_SmtpManager)
{
  m_SmtpManager = p_SmtpManager;
}

void Ui::ResetImapManager()
{
  m_ImapManager.reset();
}

void Ui::ResetSmtpManager()
{
  m_SmtpManager.reset();
}

void Ui::SetTrashFolder(const std::string &p_TrashFolder)
{
  m_TrashFolder = p_TrashFolder;
}

bool Ui::IsConnected()
{
  std::lock_guard<std::mutex> lock(m_Mutex);
  return m_Status.IsSet(Status::FlagConnected);
}

std::string Ui::GetKeyDisplay(int p_Key)
{
  if (p_Key == '\n')
  {
    return "Re";
  }
  else if ((p_Key >= 0x0) && (p_Key <= 0x1F))
  {
    return "^" + std::string(1, (char)p_Key + 0x40);
  }
  else if (p_Key == ',')
  {
    return "<";
  }
  else if (p_Key == '.')
  {
    return ">";
  }
  else if ((p_Key >= 'a') && (p_Key <= 'z'))
  {
    return std::string(1, std::toupper((char)p_Key));
  }
  else
  {
    return std::string(1, (char)p_Key);
  }

  return "??";
}

std::string Ui::GetStatusStr()
{
  std::lock_guard<std::mutex> lock(m_Mutex);
  return m_Status.ToString(m_ShowProgress);
}

std::string Ui::GetStateStr()
{
  switch (m_State)
  {
    case StateViewMessageList: return "Folder: " + m_CurrentFolder;
    case StateViewMessage: return std::string("Message ") + (m_Plaintext ? "plain" : "html");
    case StateGotoFolder: return "Goto Folder";
    case StateMoveToFolder: return "Move To Folder";
    case StateComposeMessage: return "Compose";
    case StateReplyMessage: return "Reply";
    case StateForwardMessage: return "Forward";
    case StateAddressList: return "Address Book";
    case StateViewPartList: return "Message Parts";
    default: return "Unknown State";
  }
}

bool Ui::IsValidTextKey(int p_Key)
{
  return ((p_Key >= 0x20) || (p_Key == 0x9) || (p_Key == 0xA));
}

void Ui::SendComposedMessage()
{
  SmtpManager::Action action;
  action.m_To = Util::ToString(m_ComposeHeaderStr.at(0));
  action.m_Cc = Util::ToString(m_ComposeHeaderStr.at(1));
  action.m_Att = Util::ToString(m_ComposeHeaderStr.at(2));
  action.m_Subject = Util::ToString(m_ComposeHeaderStr.at(3));
  action.m_Body = Util::ToString(Util::Join(m_ComposeMessageLines));
  action.m_RefMsgId = m_ComposeHeaderRef;

  m_SmtpManager->AsyncAction(action);
}

bool Ui::DeleteMessage()
{
  if (!m_TrashFolder.empty())
  {
    ImapManager::Action action;
    action.m_Folder = m_CurrentFolder;
    action.m_Uids.insert(m_MessageListCurrentUid[m_CurrentFolder]);
    action.m_MoveDestination = m_TrashFolder;
    m_ImapManager->AsyncAction(action);

    m_HasRequestedUids[m_TrashFolder] = false;

    {
      std::lock_guard<std::mutex> lock(m_Mutex);
      RemoveUidDate(m_CurrentFolder, action.m_Uids);
      m_Uids[m_CurrentFolder] = m_Uids[m_CurrentFolder] - action.m_Uids;
      m_Uids[m_TrashFolder] = m_Uids[m_TrashFolder] + action.m_Uids;
      m_Headers[m_CurrentFolder] = m_Headers[m_CurrentFolder] - action.m_Uids;
    }

    m_MessageViewLineOffset = 0;
    UpdateUidFromIndex(true /* p_UserTriggered */);    

    bool isMsgDateUidsEmpty = false;
    {
      std::lock_guard<std::mutex> lock(m_Mutex);
      isMsgDateUidsEmpty = m_MsgDateUids[m_CurrentFolder].empty();
    }

    if (isMsgDateUidsEmpty)
    {
      SetState(StateViewMessageList);
    }
    
    return true;
  }
  else
  {
    SetDialogMessage("Trash folder not configured");
    return false;
  }
}

void Ui::ToggleUnseen()
{
  std::map<uint32_t, uint32_t> flags;
  {
    std::lock_guard<std::mutex> lock(m_Mutex);
    flags = m_Flags[m_CurrentFolder];
  }
  uint32_t uid = m_MessageListCurrentUid[m_CurrentFolder];
  bool oldSeen = ((flags.find(uid) != flags.end()) && (Flag::GetSeen(flags.at(uid))));
  bool newSeen = !oldSeen;

  ImapManager::Action action;
  action.m_Folder = m_CurrentFolder;
  action.m_Uids.insert(m_MessageListCurrentUid[m_CurrentFolder]);
  action.m_SetSeen = newSeen;
  action.m_SetUnseen = !newSeen;
  m_ImapManager->AsyncAction(action);

  {
    std::lock_guard<std::mutex> lock(m_Mutex);
    Flag::SetSeen(m_Flags[m_CurrentFolder][uid], newSeen);
  }
}

void Ui::MarkSeen()
{
  std::map<uint32_t, uint32_t> flags;
  {
    std::lock_guard<std::mutex> lock(m_Mutex);
    flags = m_Flags[m_CurrentFolder];
  }
  uint32_t uid = m_MessageListCurrentUid[m_CurrentFolder];
  bool oldSeen = ((flags.find(uid) != flags.end()) && (Flag::GetSeen(flags.at(uid))));

  if (oldSeen) return;

  bool newSeen = true;

  ImapManager::Action action;
  action.m_Folder = m_CurrentFolder;
  action.m_Uids.insert(m_MessageListCurrentUid[m_CurrentFolder]);
  action.m_SetSeen = newSeen;
  action.m_SetUnseen = !newSeen;
  m_ImapManager->AsyncAction(action);

  {
    std::lock_guard<std::mutex> lock(m_Mutex);
    Flag::SetSeen(m_Flags[m_CurrentFolder][uid], newSeen);
  }
}

void Ui::UpdateUidFromIndex(bool p_UserTriggered)
{
  std::lock_guard<std::mutex> lock(m_Mutex);
  auto& msgDateUids = m_MsgDateUids[m_CurrentFolder];

  m_MessageListCurrentIndex[m_CurrentFolder] =
    Util::Bound(0, m_MessageListCurrentIndex[m_CurrentFolder], (int)msgDateUids.size() - 1);
  if (msgDateUids.size() > 0)
  {
    m_MessageListCurrentUid[m_CurrentFolder] =
      std::prev(msgDateUids.end(), m_MessageListCurrentIndex[m_CurrentFolder] + 1)->second;
  }
  else
  {
    m_MessageListCurrentUid[m_CurrentFolder] = -1;
  }

  m_MessageListUidSet[m_CurrentFolder] = p_UserTriggered;

  LOG_DEBUG("current uid = %d, idx = %d", m_MessageListCurrentUid[m_CurrentFolder], m_MessageListCurrentIndex[m_CurrentFolder]);
}

void Ui::UpdateIndexFromUid()
{
  bool found = false;

  {
    std::lock_guard<std::mutex> lock(m_Mutex);

    if (m_MessageListUidSet[m_CurrentFolder])
    {
      auto& msgDateUids = m_MsgDateUids[m_CurrentFolder];

      for (auto it = msgDateUids.rbegin(); it != msgDateUids.rend(); ++it)
      {
        if ((int32_t)it->second == m_MessageListCurrentUid[m_CurrentFolder])
        {
          m_MessageListCurrentIndex[m_CurrentFolder] = std::distance(msgDateUids.rbegin(), it);
          found = true;
          break;
        }
      }
    }
  }

  if (!found)
  {
    UpdateUidFromIndex(false /* p_UserTriggered */);
  }

  LOG_DEBUG("current uid = %d, idx = %d", m_MessageListCurrentUid[m_CurrentFolder], m_MessageListCurrentIndex[m_CurrentFolder]);
}

void Ui::AddUidDate(const std::string& p_Folder, const std::map<uint32_t, Header>& p_UidHeaders)
{
  auto& msgDateUids = m_MsgDateUids[p_Folder];
  auto& msgUidDates = m_MsgUidDates[p_Folder];

  for (auto it = p_UidHeaders.begin(); it != p_UidHeaders.end(); ++it)
  {
    const uint32_t uid = it->first;
    const std::string& date = m_Headers[p_Folder][uid].GetDate();
    std::string dateUid = date + std::to_string(uid);

    if (uid == 0)
    {
      LOG_WARNING("skip add date = %s, uid = %d pair", date.c_str(), uid);
      continue;
    }

    LOG_DEBUG("add date = %s, uid = %d pair", date.c_str(), uid);

    auto ret = msgDateUids.insert(std::pair<std::string, uint32_t>(dateUid, uid));
    if (ret.second)
    {
      msgUidDates.insert(std::pair<uint32_t, std::string>(uid, dateUid));
    }
  }  
}

void Ui::RemoveUidDate(const std::string& p_Folder, const std::set<uint32_t>& p_Uids)
{
  auto& msgDateUids = m_MsgDateUids[p_Folder];
  auto& msgUidDates = m_MsgUidDates[p_Folder];

  for (auto it = p_Uids.begin(); it != p_Uids.end(); ++it)
  {
    const uint32_t uid = *it;
    const std::string& date = m_Headers[p_Folder][uid].GetDate();
    std::string dateUid = date + std::to_string(uid);

    if (uid == 0)
    {
      LOG_WARNING("skip del date = %s, uid = %d pair", date.c_str(), uid);
      continue;
    }

    LOG_DEBUG("del date = %s, uid = %d pair", date.c_str(), uid);

    auto msgDateUid = msgDateUids.find(dateUid);
    if (msgDateUid != msgDateUids.end())
    {
      msgDateUids.erase(msgDateUid);
    }

    auto msgUidDate = msgUidDates.find(uid);
    if (msgUidDate != msgUidDates.end())
    {
      msgUidDates.erase(msgUidDate);
    }
  }
}

void Ui::ComposeMessagePrevLine()
{
  if (m_ComposeMessageWrapLine > 0)
  {
    if ((int)m_ComposeMessageLines[m_ComposeMessageWrapLine - 1].size() >
        m_ComposeMessageWrapPos)
    {
      int stepsBack = m_ComposeMessageWrapPos + 1 +
        (m_ComposeMessageLines[m_ComposeMessageWrapLine - 1].size() -
         m_ComposeMessageWrapPos);
      m_ComposeMessagePos = Util::Bound(0, m_ComposeMessagePos - stepsBack,
                                        (int)m_ComposeMessageStr.size());
    }
    else
    {
      int stepsBack = m_ComposeMessageWrapPos + 1;
      m_ComposeMessagePos = Util::Bound(0, m_ComposeMessagePos - stepsBack,
                                        (int)m_ComposeMessageStr.size());
    }
  }
  else
  {
    m_IsComposeHeader = true;
  }
}

void Ui::ComposeMessageNextLine()
{
  if ((m_ComposeMessageWrapLine + 1) < (int)m_ComposeMessageLines.size())
  {
    int stepsForward = m_ComposeMessageLines[m_ComposeMessageWrapLine].size() -
      m_ComposeMessageWrapPos + 1;
    if ((int)m_ComposeMessageLines[m_ComposeMessageWrapLine + 1].size() >
        m_ComposeMessageWrapPos)
    {
      stepsForward += m_ComposeMessageWrapPos;
    }
    else
    {
      stepsForward += m_ComposeMessageLines[m_ComposeMessageWrapLine + 1].size();
    }
    m_ComposeMessagePos = Util::Bound(0, m_ComposeMessagePos + stepsForward,
                                      (int)m_ComposeMessageStr.size());
  }
  else if ((int)m_ComposeMessageLines.size() > 0)
  {
    int stepsForward = m_ComposeMessageLines[m_ComposeMessageWrapLine].size() -
      m_ComposeMessageWrapPos;
    m_ComposeMessagePos = Util::Bound(0, m_ComposeMessagePos + stepsForward,
                                      (int)m_ComposeMessageStr.size());
  }
}

int Ui::ReadKeyBlocking()
{
  while (true)
  {
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    int maxfd = STDIN_FILENO;
    struct timeval tv = {1, 0};
    int rv = select(maxfd + 1, &fds, NULL, NULL, &tv);

    if (rv == 0) continue;

    if (FD_ISSET(STDIN_FILENO, &fds))
    {
      wint_t key = 0;
      get_wch(&key);

      return key;
    }
  }
}

bool Ui::PromptConfirmCancelCompose()
{
  werase(m_DialogWin);

  const std::string& dispStr = "Cancel message (y/n)?";
  int x = std::max((m_ScreenWidth - (int)dispStr.size() - 1) / 2, 0);
  wattron(m_DialogWin, A_REVERSE);
  mvwprintw(m_DialogWin, 0, x, " %s ", dispStr.c_str());
  wattroff(m_DialogWin, A_REVERSE);

  wrefresh(m_DialogWin);

  int key = ReadKeyBlocking();

  return ((key == 'y') || (key == 'Y'));
}

bool Ui::PromptString(const std::string& p_Prompt, std::string& p_Entry)
{
  if (m_HelpEnabled)
  {
    werase(m_HelpWin);
    static std::vector<std::vector<std::string>> savePartHelp =
      {
       {
        GetKeyDisplay(KEY_RETURN), "Save",
       },
       {
        GetKeyDisplay(m_KeyCancel), "Cancel",
       }
      };

    DrawHelpText(savePartHelp);

    wrefresh(m_HelpWin);
  }

  curs_set(1);

  m_FilenameEntryString = Util::ToWString(p_Entry);
  m_FilenameEntryStringPos = m_FilenameEntryString.size();

  bool rv = false;
  while (true)
  {
    werase(m_DialogWin);

    const std::string& dispStr = p_Prompt + Util::ToString(m_FilenameEntryString);
    mvwprintw(m_DialogWin, 0, 3, "%s", dispStr.c_str());
    
    leaveok(m_DialogWin, false);
    wmove(m_DialogWin, 0, 3 + p_Prompt.size() + m_FilenameEntryStringPos);
    wrefresh(m_DialogWin);
    leaveok(m_DialogWin, true);

    int key = ReadKeyBlocking();
    if (key == m_KeyCancel)
    {
      rv = false;
      break;
    }
    else if (key == KEY_RETURN)
    {
      p_Entry = Util::ToString(m_FilenameEntryString);
      rv = true;
      break;
    }
    else if (key == KEY_LEFT)
    {
      m_FilenameEntryStringPos = Util::Bound(0, m_FilenameEntryStringPos - 1,
                                             (int)m_FilenameEntryString.size());
    }
    else if (key == KEY_RIGHT)
    {
      m_FilenameEntryStringPos = Util::Bound(0, m_FilenameEntryStringPos + 1,
                                           (int)m_FilenameEntryString.size());
    }
    else if ((key == KEY_UP) || (key == KEY_DOWN) ||
             (key == KEY_PPAGE) || (key == KEY_NPAGE))
    {
      // ignore
    }
    else if (key == KEY_SYS_BACKSPACE)
    {
      if (m_FilenameEntryStringPos > 0)
      {
        m_FilenameEntryString.erase(--m_FilenameEntryStringPos, 1);
      }
    }
    else if (key == KEY_DC)
    {
      if (m_FilenameEntryStringPos < (int)m_FilenameEntryString.size())
      {
        m_FilenameEntryString.erase(m_FilenameEntryStringPos, 1);
      }
    }
    else if (IsValidTextKey(key))
    {
      m_FilenameEntryString.insert(m_FilenameEntryStringPos++, 1, key);
    }
  }

  curs_set(0);
  return rv;
}

bool Ui::CurrentMessageBodyAvailable()
{
  std::lock_guard<std::mutex> lock(m_Mutex);
  const std::map<uint32_t, Body>& bodys = m_Bodys[m_CurrentFolder];
  std::map<uint32_t, Body>::const_iterator bit = bodys.find(m_MessageListCurrentUid[m_CurrentFolder]);
  return (bit != bodys.end());
}

void Ui::InvalidateUiCache(const std::string& p_Folder)
{
  m_HasRequestedUids[p_Folder] = false;

  std::lock_guard<std::mutex> lock(m_Mutex);
  std::map<uint32_t, uint32_t>& flags = m_Flags[p_Folder];
  flags.clear();
  std::set<uint32_t>& requestedFlags = m_RequestedFlags[p_Folder];
  requestedFlags.clear();
}
