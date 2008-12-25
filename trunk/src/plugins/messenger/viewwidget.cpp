#include <QDebug>
#include "viewwidget.h"

#include <QTextFrame>
#include <QTextTable>
#include <QScrollBar>
#include <QResizeEvent>

ViewWidget::ViewWidget(IMessenger *AMessenger, const Jid &AStreamJid, const Jid &AContactJid)
{
  ui.setupUi(this);
  ui.tedViewer->setWordWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);

  FMessenger = AMessenger;
  FStreamJid = AStreamJid;
  FContactJid = AContactJid;

  FOptions = 0;
  FSetScrollToMax = false;
  FShowKind = ChatMessage;

  ui.tedViewer->installEventFilter(this);
}

ViewWidget::~ViewWidget()
{

}

void ViewWidget::setStreamJid(const Jid &AStreamJid)
{
  if (AStreamJid != FStreamJid)
  {
    Jid befour = FStreamJid;
    FStreamJid = AStreamJid;
    if (FShowKind == ChatMessage)
    {
      setColorForJid(FStreamJid,colorForJid(befour));
      setColorForJid(befour,QColor());
      setNickForJid(FStreamJid,nickForJid(befour));
      setNickForJid(befour,QString());
    }
    emit streamJidChanged(befour);
  }
}

void ViewWidget::setContactJid(const Jid &AContactJid)
{
  if (AContactJid != FContactJid)
  {
    Jid befour = FContactJid;
    FContactJid = AContactJid;
    if (FShowKind == ChatMessage)
    {
      setColorForJid(FContactJid,colorForJid(befour));
      setColorForJid(befour,QColor());
      setNickForJid(FContactJid,nickForJid(befour));
      setNickForJid(befour,QString());
    }
    emit contactJidChanged(AContactJid);
  }
}

void ViewWidget::setShowKind(ShowKind AKind)
{
  FShowKind = AKind;
  if (FShowKind == ChatMessage)
  {
    setColorForJid(FStreamJid,FJid2Color.value(FStreamJid,Qt::red));
    setColorForJid(FContactJid,FJid2Color.value(FContactJid,Qt::blue));
  }
}

void ViewWidget::showMessage(const Message &AMessage)
{
  if (FShowKind == NormalMessage)
  {
    QTextDocument messageDoc;
    messageDoc.setDefaultFont(document()->defaultFont());
    FMessenger->messageToText(&messageDoc,AMessage);
    showCustomMessage(FMessenger->checkOption(IMessenger::ShowHTML) ? messageDoc.toHtml() : messageDoc.toPlainText());
  }
  else
  {
    Jid authorJid = AMessage.from().isEmpty() ? FStreamJid : AMessage.from();
    QString authorNick = FJid2Nick.value(authorJid,FShowKind == GroupChatMessage ? authorJid.resource() : authorJid.node());
    QColor authorColor = colorForJid(authorJid);

    QTextDocument messageDoc;
    FMessenger->messageToText(&messageDoc,AMessage);
    
    if (processMeCommand(&messageDoc,authorNick,authorColor))
      authorNick.clear();

    if (FMessenger->checkOption(IMessenger::ShowHTML))
      showCustomMessage(messageDoc.toHtml(),AMessage.dateTime(),authorNick,authorColor);
    else
      showCustomMessage(messageDoc.toPlainText(),AMessage.dateTime(),authorNick,authorColor);
  }

  emit messageShown(AMessage);
}

void ViewWidget::showCustomMessage(const QString &AHtml, const QDateTime &ATime, const QString &ANick, const QColor &ANickColor)
{
  if (FShowKind != NormalMessage)
  {
    QTextCursor cursor = document()->rootFrame()->lastCursorPosition();
    bool scrollAtEnd = textBrowser()->verticalScrollBar()->sliderPosition() == textBrowser()->verticalScrollBar()->maximum();


    QTextTableFormat tableFormat;
    tableFormat.setBorder(0);
    tableFormat.setBorderStyle(QTextTableFormat::BorderStyle_None);
    QTextTable *table = cursor.insertTable(1,2,tableFormat);

    if (ATime.isValid() && FMessenger->checkOption(IMessenger::ShowDateTime))
    {
      QTextCharFormat timeFormat;
      timeFormat.setForeground(Qt::gray);
      QString timeString = QDateTime::currentDateTime().date()==ATime.date() ? tr("hh:mm") : tr("dd.MM hh:mm");
      table->cellAt(0,0).lastCursorPosition().insertText(QString("[%1]").arg(ATime.toString(timeString)),timeFormat);
    }

    if (!ANick.isEmpty())
    {
      QTextCharFormat nickFormat;
      nickFormat.setForeground(ANickColor);
      table->cellAt(0,0).lastCursorPosition().insertText(QString("[%1]").arg(ANick),nickFormat);
    }

    table->cellAt(0,1).lastCursorPosition().insertHtml(getHtmlBody(AHtml.trimmed()));

    if (scrollAtEnd)
      textBrowser()->verticalScrollBar()->setSliderPosition(textBrowser()->verticalScrollBar()->maximum());
  }
  else
    document()->setHtml(getHtmlBody(AHtml));
}

void ViewWidget::showCustomHtml(const QString &AHtml)
{
  QTextCursor cursor = document()->rootFrame()->lastCursorPosition();
  bool scrollAtEnd = textBrowser()->verticalScrollBar()->sliderPosition() == textBrowser()->verticalScrollBar()->maximum();
  
  cursor.insertHtml(AHtml);
  cursor.insertBlock();

  if (scrollAtEnd)
    textBrowser()->verticalScrollBar()->setSliderPosition(textBrowser()->verticalScrollBar()->maximum());

  emit customHtmlShown(AHtml);
}

void ViewWidget::setColorForJid(const Jid &AJid, const QColor &AColor)
{
  if (AColor.isValid())
    FJid2Color.insert(AJid,AColor);
  else
    FJid2Color.remove(AJid);
  emit colorForJidChanged(AJid,AColor);
}

void ViewWidget::setNickForJid(const Jid &AJid, const QString &ANick)
{
  if (!ANick.isNull())
    FJid2Nick.insert(AJid,ANick);
  else
    FJid2Nick.remove(AJid);
  emit nickForJidChanged(AJid,ANick);
}

QString ViewWidget::getHtmlBody(const QString &AHtml)
{
  QRegExp bodyStart("<body.*>");
  QRegExp bodyEnd("</body>");
  int start = AHtml.indexOf(bodyStart);
  int end = AHtml.lastIndexOf(bodyEnd);
  if (start >=0 && end > start)
  {
    start = AHtml.indexOf(">",start)+1;
    return AHtml.mid(start,end-start);
  }
  return AHtml;
}

bool ViewWidget::processMeCommand(QTextDocument *ADocument, const QString &ANick, const QColor &AColor)
{
  bool found = false;
  QRegExp regexp("^/me\\s");
  for (QTextCursor cursor = ADocument->find(regexp); !cursor.isNull();  cursor = ADocument->find(regexp,cursor))
  {
    QTextCharFormat nickFormat;
    nickFormat.setForeground(AColor);
    cursor.insertText("* "+ANick+" ",nickFormat);

    QTextCharFormat lineFormat;
    lineFormat.setFontItalic(true);
    cursor.select(QTextCursor::LineUnderCursor);
    cursor.mergeCharFormat(lineFormat);

    found = true;
  }
  return found;
}

bool ViewWidget::eventFilter(QObject *AWatched, QEvent *AEvent)
{
  if (AWatched == ui.tedViewer && AEvent->type() == QEvent::Resize)
  {
    QScrollBar *scrollBar = ui.tedViewer->verticalScrollBar();
    FSetScrollToMax = FSetScrollToMax || (scrollBar->sliderPosition() == scrollBar->maximum());
  }
  else if (AWatched == ui.tedViewer && AEvent->type() == QEvent::Paint)
  {
    if (FSetScrollToMax)
    {
      QScrollBar *scrollBar = ui.tedViewer->verticalScrollBar();
      scrollBar->setSliderPosition(scrollBar->maximum());
      FSetScrollToMax = false;
    }
  }

  return QWidget::eventFilter(AWatched,AEvent);
}

