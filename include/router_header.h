
#ifndef __ROUTER_HEADER__
#define __ROUTER_HEADER__

typedef struct tagRouterHeader
{
    unsigned int PkgLen; // 4
    uint64_t UserID;     // 8
    unsigned int CmdID;  // 4
    unsigned int SrcID;  // 4
    unsigned int DstID;  // 4
    unsigned int SN;     // 4
    short Ret;           // 2

    tagRouterHeader()
    {
        memset(this, 0x0, sizeof(tagRouterHeader));
    }
    
    int GetHeaderLen()
    {
        return 30;
    }
    
    int Write(char *Buff)
    {
        int Offset = 0;
        Offset += mmlib::CBuffTool::WriteInt(Buff + Offset, PkgLen);
        Offset += mmlib::CBuffTool::WriteLong(Buff + Offset, UserID);
        Offset += mmlib::CBuffTool::WriteInt(Buff + Offset, CmdID);
        Offset += mmlib::CBuffTool::WriteInt(Buff + Offset, SrcID);
        Offset += mmlib::CBuffTool::WriteInt(Buff + Offset, DstID);
        Offset += mmlib::CBuffTool::WriteInt(Buff + Offset, SN);
        Offset += mmlib::CBuffTool::WriteShort(Buff + Offset, Ret);
        
        return Offset;
    }

    int Read(const char *Buff)
    {
        int Offset = 0;
        Offset += mmlib::CBuffTool::ReadInt(Buff + Offset, PkgLen);
        Offset += mmlib::CBuffTool::ReadLong(Buff + Offset, UserID);
        Offset += mmlib::CBuffTool::ReadInt(Buff + Offset, CmdID);
        Offset += mmlib::CBuffTool::ReadInt(Buff + Offset, SrcID);
        Offset += mmlib::CBuffTool::ReadInt(Buff + Offset, DstID);
        Offset += mmlib::CBuffTool::ReadInt(Buff + Offset, SN);
        Offset += mmlib::CBuffTool::ReadShort(Buff + Offset, Ret);
        
        return Offset;
    }
    
}RouterHeader;


typedef struct tagAppHeader
{
    unsigned int UserID;
    unsigned int CmdID;
    unsigned int DstID;
    
    tagAppHeader()
    {
        memset(this, 0x0, sizeof(tagAppHeader));
    }
}AppHeader;

#endif