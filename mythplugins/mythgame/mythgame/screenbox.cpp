#include <qlayout.h>
#include <qapplication.h>
#include <qsqldatabase.h>
#include <qcursor.h>
#include <iostream>
#include <stdlib.h>

using namespace std;

#include "rominfo.h"
#include "screenbox.h"
#include "treeitem.h"
#include "gamehandler.h"
#include "extendedlistview.h"

#include <mythtv/mythcontext.h>

ScreenBox::ScreenBox(MythContext *context, QSqlDatabase *ldb, QString &paths,
                     QWidget *parent, const char *name)
           : QDialog(parent, name)
{
    db = ldb;
    m_context = context;

    int screenheight = 0, screenwidth = 0;
    float wmult = 0, hmult = 0;

    context->GetScreenSettings(screenwidth, wmult, screenheight, hmult);

    setGeometry(0, 0, screenwidth, screenheight);
    setFixedSize(QSize(screenwidth, screenheight));

    setFont(QFont("Arial", (int)(m_context->GetMediumFontSize() * hmult),
                  QFont::Bold));
    setCursor(QCursor(Qt::BlankCursor));

    QVBoxLayout *vbox = new QVBoxLayout(this, (int)(5 * wmult), 
                                        (int)(5 * wmult));

    mGameLabel = new QLabel(this);
    mGameLabel->setFixedHeight((int)(23 * hmult));
    mGameLabel->setAlignment(Qt::AlignHCenter);
    mGameLabel->setText("");

    ExtendedListView *listview = new ExtendedListView(this);
    listview->setFixedHeight((int)(170 * hmult));
    listview->addColumn("Select genre", -1);

    listview->setSorting(-1);
    listview->setRootIsDecorated(false);
    listview->setAllColumnsShowFocus(true);
    listview->setColumnWidthMode(0, QListView::Maximum);
    listview->setResizeMode(QListView::LastColumn);

    PicFrame = new SelectFrame(context, this);
    PicFrame->setFixedWidth((int)(790 * wmult));
    PicFrame->setFixedHeight((int)(350 * hmult));
    PicFrame->setPaletteBackgroundColor(QColor(255,255,255));
    PicFrame->setFocusPolicy(QWidget::NoFocus);

    connect(listview, SIGNAL(currentChanged(QListViewItem *)), this,
            SLOT(setImages(QListViewItem*)));
    connect(PicFrame, SIGNAL(gameChanged(const QString&)), mGameLabel,
            SLOT(setText(const QString&)));
    connect(listview, SIGNAL(KeyPressed(QListViewItem *, int)), this,
            SLOT(handleKey(QListViewItem *, int)));

    fillList(listview, paths);

    vbox->addWidget(listview, 0);
    vbox->addWidget(mGameLabel, 1);
    vbox->addWidget(PicFrame, 0);

    PicFrame->setDimensions();
    PicFrame->setButtons(NULL);
    PicFrame->setUpdatesEnabled(true);

    listview->setCurrentItem(listview->firstChild());
    listview->setSelected(listview->firstChild(), true);
}

void ScreenBox::Show()
{
    showFullScreen();
    setActiveWindow();
}

void ScreenBox::fillList(QListView *listview, QString &paths)
{
    QString templevel = "system";
    QString temptitle = "All Games";
    TreeItem *allgames = new TreeItem(m_context, listview, temptitle,
                                      templevel, NULL);
    
    QStringList lines = QStringList::split(" ", paths);
    int Levels = lines.count();

    if(lines[Levels - 1] == "gamename" && Levels > 1)
        leafLevel = lines[Levels - 2];
    else
        leafLevel = lines[Levels - 1];

    QString first = lines.front();

    char thequery[1024];
    sprintf(thequery, "SELECT DISTINCT %s FROM gamemetadata ORDER BY %s DESC;",
                      first.ascii(), first.ascii());

    QSqlQuery query = db->exec(thequery);

    if (query.isActive() && query.numRowsAffected() > 0)
    {
        while (query.next())
        {
            QString current = query.value(0).toString();

            QString querystr = first;
            QString matchstr = first + " = \"" + current + "\"";           
 
            QStringList::Iterator line = lines.begin();
            ++line;
            int num = 1;

            QString level = *line;

            RomInfo *rinfo = new RomInfo();
            rinfo->setField(first, current);

            TreeItem *item = new TreeItem(m_context, allgames, current,
                                          first, rinfo);
            fillNextLevel(level, num, querystr, matchstr, line, lines,
                          item);
        }
    }

    listview->setOpen(allgames, true);
}

void ScreenBox::fillNextLevel(QString level, int num, QString querystr, 
                                QString matchstr, QStringList::Iterator line,
                                QStringList lines, TreeItem *parent)
{
    if (level == "")
        return;

    QString orderstr = querystr;
 
    bool isleaf = false; 
    if (level == leafLevel)
    {
        isleaf = true;
    }
    querystr += "," + level;
    orderstr += "," + level;

    char thequery[1024];
      sprintf(thequery, "SELECT DISTINCT %s FROM gamemetadata WHERE %s "
                        "ORDER BY %s DESC;",
                        querystr.ascii(), matchstr.ascii(), orderstr.ascii());
                      
    QSqlQuery query = db->exec(thequery);
  
    ++line; 
    if (query.isActive() && query.numRowsAffected() > 0)
    {
        while (query.next())
        {
            QString current = query.value(num).toString();
            QString matchstr2 = matchstr + " AND " + level + " = \"" + 
                                current + "\"";

            RomInfo *parentinfo = parent->getRomInfo();
            RomInfo *rinfo;
            rinfo = new RomInfo(*parentinfo);
            rinfo->setField(level, current);
            TreeItem *item = new TreeItem(m_context, parent, current, level,
                                          rinfo);

            if (line != lines.end() && *line != "gamename")
                fillNextLevel(*line, num + 1, querystr, matchstr2, line, lines,
                              item);

            if (!isleaf)
                checkParent(item);
        }
    }
}

void ScreenBox::checkParent(QListViewItem *item)
{
    TreeItem *tcitem = (TreeItem *)item;

    TreeItem *child = (TreeItem *)tcitem->firstChild();
    if (!child)
        return;

    //bool same = true;
    while (child)
    {
        child = (TreeItem *)child->nextSibling();
    }

    if (tcitem->parent())
    {
        checkParent(tcitem->parent());
    }
}

void ScreenBox::setImages(QListViewItem *item)
{
    TreeItem *tcitem = (TreeItem *)item;
    if(tcitem->getLevel() == leafLevel)
    {
        PicFrame->setFocusPolicy(QWidget::TabFocus);
        RomInfo *romdata = tcitem->getRomInfo();
        char thequery[1024];
        QString matchstr = "system = \"" + romdata->System() + "\"";
        if(romdata->Genre() != "")
            matchstr+= " AND genre = \"" + romdata->Genre() + "\"";
        if(romdata->Year() != 0)
        {
            QString year;
            year.sprintf("%d",romdata->Year());
            matchstr+= " AND year = \"" + year + "\"";
        }
        sprintf(thequery, "SELECT DISTINCT gamename FROM gamemetadata WHERE %s "
                        "ORDER BY gamename DESC;",
                        matchstr.latin1());
        QSqlQuery query = db->exec(thequery);
        if (query.isActive() && query.numRowsAffected() > 0)
        {
            RomInfo *rinfo;
            QPtrList<RomInfo> *romlist = new QPtrList<RomInfo>;
            while (query.next())
            {
                rinfo = GameHandler::CreateRomInfo(m_context, romdata);
                rinfo->setField("gamename",query.value(0).toString());
                rinfo->fillData(db);
                romlist->append(rinfo);
            }
            PicFrame->setRomlist(romlist);
        }
    }
    else
    {
        PicFrame->clearButtons();
        PicFrame->setFocusPolicy(QWidget::NoFocus);
    }
}

void ScreenBox::editSettings(QListViewItem *item)
{
    TreeItem *tcitem = (TreeItem *)item;

    if("system" == tcitem->getLevel())
    {
        GameHandler::EditSystemSettings(m_context, this, tcitem->getRomInfo());
    }
}

void ScreenBox::handleKey(QListViewItem *item, int key)
{
  TreeItem *tcitem = (TreeItem *)item;
  switch(key)
  {
  case Key_E:
    editSettings(tcitem);
    break;
  default:
    break;
  }
}                
