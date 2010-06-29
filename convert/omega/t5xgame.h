#ifndef _T5XGAME_H_
#define _T5XGAME_H_

class T5X_LOCKEXP
{
public:
    enum
    {
        le_is,
        le_carry,
        le_indirect,
        le_owner,
        le_and,
        le_or,
        le_not,
        le_ref,
        le_attr1,
        le_attr2,
        le_eval1,
        le_eval2,
        le_none,
    } m_op;

    T5X_LOCKEXP *m_le[2];
    int          m_dbRef;
    char        *m_p[2];

    void SetIs(T5X_LOCKEXP *p)
    {
        m_op = le_is;
        m_le[0] = p;
    }
    void SetCarry(T5X_LOCKEXP *p)
    {
        m_op = le_carry;
        m_le[0] = p;
    }
    void SetIndir(T5X_LOCKEXP *p)
    {
        m_op = le_indirect;
        m_le[0] = p;
    }
    void SetOwner(T5X_LOCKEXP *p)
    {
        m_op = le_owner;
        m_le[0] = p;
    }
    void SetAnd(T5X_LOCKEXP *p, T5X_LOCKEXP *q)
    {
        m_op = le_and;
        m_le[0] = p;
        m_le[1] = q;
    }
    void SetOr(T5X_LOCKEXP *p, T5X_LOCKEXP *q)
    {
        m_op = le_or;
        m_le[0] = p;
        m_le[1] = q;
    }
    void SetNot(T5X_LOCKEXP *p)
    {
        m_op = le_not;
        m_le[0] = p;
    }
    void SetRef(int dbRef)
    {
        m_op = le_ref;
        m_dbRef = dbRef;
    }
    void SetAttr(char *p, char *q)
    {
        m_op = le_attr1;
        m_p[0] = p;
        m_p[1] = q;
    }
    void SetAttr(int dbRef, char *q)
    {
        m_op = le_attr2;
        m_dbRef = dbRef;
        m_p[1] = q;
    }
    void SetEval(char *p, char *q)
    {
        m_op = le_eval1;
        m_p[0] = p;
        m_p[1] = q;
    }
    void SetEval(int dbRef, char *q)
    {
        m_op = le_eval2;
        m_dbRef = dbRef;
        m_p[1] = q;
    }

    void Write(FILE *fp);

    T5X_LOCKEXP()
    {
        m_op = le_none;
        m_le[0] = m_le[1] = NULL;
        m_p[0] = m_p[1] = NULL;
        m_dbRef = 0;
    }
    ~T5X_LOCKEXP()
    {
        delete m_le[0];
        delete m_le[1];
        free(m_p[0]);
        free(m_p[1]);
        m_le[0] = m_le[1] = NULL;
        m_p[0] = m_p[1] = NULL;
    } 
};

class T5X_ATTRNAMEINFO
{
public:
    bool  m_fNumAndName;
    int   m_iNum;
    char *m_pName;
    void  SetNumAndName(int iNum, char *pName);

    void Write(FILE *fp);

    T5X_ATTRNAMEINFO()
    {
        m_fNumAndName = false;
        m_pName = NULL;
    }
    ~T5X_ATTRNAMEINFO()
    {
        free(m_pName);
        m_pName = NULL;
    }
};

class T5X_ATTRINFO
{
public:
    bool m_fNumAndValue;
    int  m_iNum;
    char *m_pValue;
    void SetNumAndValue(int iNum, char *pValue);

    void Write(FILE *fp) const;

    T5X_ATTRINFO()
    {
        m_fNumAndValue = false;
        m_pValue = NULL;
    }
    ~T5X_ATTRINFO()
    {
        free(m_pValue);
        m_pValue = NULL;
    }
};

class T5X_OBJECTINFO
{
public:
    bool m_fRef;
    int  m_dbRef;
    void SetRef(int dbRef) { m_fRef = true; m_dbRef = dbRef; }

    char *m_pName;
    void SetName(char *p);

    bool m_fLocation;
    int  m_dbLocation;
    void SetLocation(int dbLocation) { m_fLocation = true; m_dbLocation = dbLocation; }

    bool m_fContents;
    int  m_dbContents;
    void SetContents(int dbContents) { m_fContents = true; m_dbContents = dbContents; }

    bool m_fExits;
    int  m_dbExits;
    void SetExits(int dbExits) { m_fExits = true; m_dbExits = dbExits; }

    bool m_fNext;
    int  m_dbNext;
    void SetNext(int dbNext) { m_fNext = true; m_dbNext = dbNext; }

    bool m_fParent;
    int  m_dbParent;
    void SetParent(int dbParent) { m_fParent = true; m_dbParent = dbParent; }

    bool m_fOwner;
    int  m_dbOwner;
    void SetOwner(int dbOwner) { m_fOwner = true; m_dbOwner = dbOwner; }

    bool m_fZone;
    int  m_dbZone;
    void SetZone(int dbZone) { m_fZone = true; m_dbZone = dbZone; }

    bool m_fPennies;
    int  m_iPennies;
    void SetPennies(int iPennies) { m_fPennies = true; m_iPennies = iPennies; }

    bool m_fFlags1;
    int  m_iFlags1;
    void SetFlags1(int iFlags1) { m_fFlags1 = true; m_iFlags1 = iFlags1; }

    bool m_fFlags2;
    int  m_iFlags2;
    void SetFlags2(int iFlags2) { m_fFlags2 = true; m_iFlags2 = iFlags2; }

    bool m_fFlags3;
    int  m_iFlags3;
    void SetFlags3(int iFlags3) { m_fFlags3 = true; m_iFlags3 = iFlags3; }

    bool m_fPowers1;
    int  m_iPowers1;
    void SetPowers1(int iPowers1) { m_fPowers1 = true; m_iPowers1 = iPowers1; }

    bool m_fPowers2;
    int  m_iPowers2;
    void SetPowers2(int iPowers2) { m_fPowers2 = true; m_iPowers2 = iPowers2; }

    bool m_fLink;
    int  m_dbLink;
    void SetLink(int dbLink) { m_fLink = true; m_dbLink = dbLink; }

    bool m_fAttrCount;
    int  m_nAttrCount;
    vector<T5X_ATTRINFO *> *m_pvai;
    void SetAttrs(int nAttrCount, vector<T5X_ATTRINFO *> *pvai);

    T5X_LOCKEXP *m_ple;
    void SetUseLock(T5X_LOCKEXP *p) { free(m_ple); m_ple = p; }

    void Write(FILE *fp, bool bWriteLock);

    T5X_OBJECTINFO()
    {
        m_fRef = false;
        m_pName = NULL;
        m_fLocation = false;
        m_fContents = false;
        m_fExits = false;
        m_fNext = false;
        m_fParent = false;
        m_fOwner = false;
        m_fZone = false;
        m_fPennies = false;
        m_fPennies = false;
        m_fAttrCount = false;
        m_pvai = NULL;
        m_ple = NULL;
    }
    ~T5X_OBJECTINFO()
    {
        free(m_pName);
        m_pName = NULL;
        if (NULL != m_pvai)
        {
            for (vector<T5X_ATTRINFO *>::iterator it = m_pvai->begin(); it != m_pvai->end(); ++it)
            {
               delete *it;
            } 
            delete m_pvai;
            m_pvai = NULL;
        }
    }
};


class T5X_GAME
{
public:
    void Validate();
    void ValidateFlags();
    void ValidateAttrNames();
    void ValidateObjects();

    void Write(FILE *fp);

    int m_flags;
    void SetFlags(int flags) { m_flags = flags; }
    int  GetFlags()          { return m_flags;  }

    bool m_fObjects;
    int  m_nObjects;
    void SetObjectCount(int nObjects) { m_fObjects = true; m_nObjects = nObjects; }

    bool m_fNextAttr;
    int  m_nNextAttr;
    void SetNextAttr(int nNextAttr) { m_fNextAttr = true; m_nNextAttr = nNextAttr; }

    bool m_fRecordPlayers;
    int  m_nRecordPlayers;
    void SetRecordPlayers(int nRecordPlayers) { m_fRecordPlayers = true; m_nRecordPlayers = nRecordPlayers; }

    vector<T5X_ATTRNAMEINFO *> m_vAttrNames;
    void AddNumAndName(int iNum, char *pName);

    vector<T5X_OBJECTINFO *> m_vObjects;
    void AddObject(T5X_OBJECTINFO *poi);

    T5X_GAME()
    {
        m_flags = 0;
        m_fObjects = false;
        m_fNextAttr = false;
        m_fRecordPlayers = false;
        m_nObjects = 0;
    }
    ~T5X_GAME()
    {
        for (vector<T5X_ATTRNAMEINFO *>::iterator it = m_vAttrNames.begin(); it != m_vAttrNames.end(); ++it)
        {
            delete *it;
        } 
        m_vAttrNames.clear();
        for (vector<T5X_OBJECTINFO *>::iterator it = m_vObjects.begin(); it != m_vObjects.end(); ++it)
        {
            delete *it;
        } 
        m_vObjects.clear();
    }
};

extern T5X_GAME g_t5xgame;

#endif
