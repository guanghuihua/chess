#include "profile_dashboard_widget.h"

#include <algorithm>

#include <QFont>
#include <QPaintEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QSizePolicy>

namespace {
constexpr qreal margin = 10.0;
constexpr qreal gap = 9.0;

void drawSectionTitle(QPainter &painter, const QRectF &rect, const QString &title)
{
    QFont font = painter.font();
    font.setPointSize(11);
    font.setBold(true);
    painter.setFont(font);
    painter.setPen(QColor(55, 50, 44));
    painter.drawText(rect, Qt::AlignLeft | Qt::AlignVCenter, title);
}
}

ProfileDashboardWidget::ProfileDashboardWidget(QWidget *parent)
    : QWidget(parent)
{
    setMinimumHeight(690);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

void ProfileDashboardWidget::setProfileData(
    const QString &userName,
    const GameDatabase::UserProfile &profile,
    const GameDatabase::TrainingStats &stats,
    const GameDatabase::TrainingSummary &training,
    const QVector<GameDatabase::GamePerformance> &recentGames)
{
    user_name_ = userName;
    profile_ = profile;
    stats_ = stats;
    training_ = training;
    recent_games_ = recentGames;
    update();
}

void ProfileDashboardWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const qreal contentWidth = width() - 2 * margin;
    const qreal cardWidth = (contentWidth - gap) / 2.0;
    const qreal cardHeight = 67.0;
    const double winRate = profile_.completedGames > 0
        ? 100.0 * profile_.wins / profile_.completedGames : 0.0;
    const double trainingAccuracy = training_.attempts > 0
        ? 100.0 * training_.correctAttempts / training_.attempts : 0.0;

    drawMetricCard(painter, QRectF(margin, margin, cardWidth, cardHeight),
                   QString::fromUtf8(u8"有效对局"),
                   QString::number(profile_.completedGames), QColor(145, 67, 48));
    drawMetricCard(painter, QRectF(margin + cardWidth + gap, margin, cardWidth, cardHeight),
                   QString::fromUtf8(u8"胜率"),
                   QString::number(winRate, 'f', 1) + "%", QColor(40, 125, 76));
    drawMetricCard(painter, QRectF(margin, margin + cardHeight + gap, cardWidth, cardHeight),
                   QString::fromUtf8(u8"平均局面损失"),
                   QString::number(profile_.averageLoss, 'f', 1), QColor(198, 132, 38));
    drawMetricCard(painter,
                   QRectF(margin + cardWidth + gap, margin + cardHeight + gap,
                          cardWidth, cardHeight),
                   QString::fromUtf8(u8"训练正确率"),
                   QString::number(trainingAccuracy, 'f', 1) + "%", QColor(67, 104, 165));

    qreal y = margin + 2 * cardHeight + 2 * gap + 8;
    drawResultChart(painter, QRectF(margin, y, contentWidth, 142));
    y += 151;
    drawErrorChart(painter, QRectF(margin, y, contentWidth, 180));
    y += 189;
    drawTrendChart(painter, QRectF(margin, y, contentWidth, 180));
}

void ProfileDashboardWidget::drawMetricCard(QPainter &painter, const QRectF &rect,
                                             const QString &title,
                                             const QString &value,
                                             const QColor &accent)
{
    painter.setPen(QPen(QColor(224, 216, 202), 1));
    painter.setBrush(QColor(255, 253, 248));
    painter.drawRoundedRect(rect, 7, 7);
    painter.setPen(Qt::NoPen);
    painter.setBrush(accent);
    painter.drawRoundedRect(QRectF(rect.left(), rect.top(), 4, rect.height()), 2, 2);

    QFont titleFont = painter.font();
    titleFont.setPointSize(9);
    painter.setFont(titleFont);
    painter.setPen(QColor(118, 109, 97));
    painter.drawText(rect.adjusted(13, 7, -6, -35), Qt::AlignLeft | Qt::AlignVCenter, title);

    QFont valueFont = painter.font();
    valueFont.setPointSize(16);
    valueFont.setBold(true);
    painter.setFont(valueFont);
    painter.setPen(QColor(42, 38, 33));
    painter.drawText(rect.adjusted(13, 26, -6, -5), Qt::AlignLeft | Qt::AlignVCenter, value);
}

void ProfileDashboardWidget::drawResultChart(QPainter &painter, const QRectF &rect) const
{
    drawSectionTitle(painter, QRectF(rect.left(), rect.top(), rect.width(), 24),
                     QString::fromUtf8(u8"对局结果分布"));
    const QRectF chartRect(rect.left() + 8, rect.top() + 31, 88, 88);
    const int total = profile_.wins + profile_.losses + profile_.draws;
    const QVector<QPair<int, QColor>> parts = {
        {profile_.wins, QColor(54, 145, 85)},
        {profile_.losses, QColor(194, 73, 61)},
        {profile_.draws, QColor(181, 151, 68)}
    };
    int startAngle = 90 * 16;
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(QColor(226, 222, 213), 14));
    painter.drawEllipse(chartRect.adjusted(7, 7, -7, -7));
    if (total > 0) {
        for (const auto &part : parts) {
            if (part.first <= 0) continue;
            const int span = -qRound(360.0 * 16 * part.first / total);
            painter.setPen(QPen(part.second, 14, Qt::SolidLine, Qt::FlatCap));
            painter.drawArc(chartRect.adjusted(7, 7, -7, -7), startAngle, span);
            startAngle += span;
        }
    }

    const QStringList labels = {
        QString::fromUtf8(u8"胜  %1").arg(profile_.wins),
        QString::fromUtf8(u8"负  %1").arg(profile_.losses),
        QString::fromUtf8(u8"和  %1").arg(profile_.draws)
    };
    qreal legendY = rect.top() + 42;
    for (int index = 0; index < labels.size(); ++index) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(parts[index].second);
        painter.drawEllipse(QPointF(rect.left() + 124, legendY + 6), 5, 5);
        painter.setPen(QColor(70, 64, 57));
        painter.drawText(QRectF(rect.left() + 138, legendY - 2, rect.width() - 145, 20),
                         Qt::AlignLeft | Qt::AlignVCenter, labels[index]);
        legendY += 27;
    }
}

void ProfileDashboardWidget::drawErrorChart(QPainter &painter, const QRectF &rect) const
{
    drawSectionTitle(painter, QRectF(rect.left(), rect.top(), rect.width(), 24),
                     QString::fromUtf8(u8"走法质量分布"));
    const QVector<int> values = {stats_.excellentMoves, stats_.inaccuracies,
                                 stats_.mistakes, stats_.blunders};
    const QStringList labels = {QString::fromUtf8(u8"优秀"), QString::fromUtf8(u8"轻微失误"),
                                QString::fromUtf8(u8"明显失误"), QString::fromUtf8(u8"严重失误")};
    const QVector<QColor> colors = {QColor(54, 145, 85), QColor(204, 166, 54),
                                    QColor(217, 126, 45), QColor(194, 73, 61)};
    const int maximum = std::max(1, *std::max_element(values.begin(), values.end()));
    const qreal labelWidth = 66;
    const qreal barLeft = rect.left() + labelWidth;
    const qreal barWidth = rect.width() - labelWidth - 35;
    for (int index = 0; index < values.size(); ++index) {
        const qreal y = rect.top() + 34 + index * 34;
        painter.setPen(QColor(92, 84, 74));
        painter.drawText(QRectF(rect.left(), y, labelWidth - 6, 21),
                         Qt::AlignRight | Qt::AlignVCenter, labels[index]);
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(237, 233, 225));
        painter.drawRoundedRect(QRectF(barLeft, y + 4, barWidth, 13), 5, 5);
        painter.setBrush(colors[index]);
        painter.drawRoundedRect(QRectF(barLeft, y + 4,
                                       barWidth * values[index] / maximum, 13), 5, 5);
        painter.setPen(QColor(80, 73, 65));
        painter.drawText(QRectF(barLeft + barWidth + 7, y, 28, 21),
                         Qt::AlignLeft | Qt::AlignVCenter, QString::number(values[index]));
    }
}

void ProfileDashboardWidget::drawTrendChart(QPainter &painter, const QRectF &rect) const
{
    drawSectionTitle(painter, QRectF(rect.left(), rect.top(), rect.width(), 24),
                     QString::fromUtf8(u8"最近对局 · 平均局面损失趋势"));
    const QRectF plot = rect.adjusted(32, 34, -12, -25);
    painter.setPen(QPen(QColor(224, 218, 207), 1));
    painter.drawLine(plot.bottomLeft(), plot.bottomRight());
    painter.drawLine(plot.topLeft(), plot.bottomLeft());
    if (recent_games_.isEmpty()) {
        painter.setPen(QColor(135, 127, 116));
        painter.drawText(plot, Qt::AlignCenter, QString::fromUtf8(u8"完成对局后显示趋势"));
        return;
    }

    double maximum = 30.0;
    for (const auto &game : recent_games_) maximum = std::max(maximum, game.averageLoss);
    QPainterPath path;
    QVector<QPointF> points;
    for (int index = 0; index < recent_games_.size(); ++index) {
        const qreal x = recent_games_.size() == 1 ? plot.center().x()
            : plot.left() + plot.width() * index / (recent_games_.size() - 1);
        const qreal y = plot.bottom() - plot.height() * recent_games_[index].averageLoss / maximum;
        points.push_back(QPointF(x, y));
        if (index == 0) path.moveTo(x, y); else path.lineTo(x, y);
    }
    painter.setPen(QPen(QColor(159, 69, 51), 2.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(path);
    for (int index = 0; index < points.size(); ++index) {
        const QColor pointColor = recent_games_[index].blunders > 0
            ? QColor(194, 73, 61) : QColor(54, 145, 85);
        painter.setPen(QPen(Qt::white, 1.5));
        painter.setBrush(pointColor);
        painter.drawEllipse(points[index], 4.5, 4.5);
    }
    painter.setPen(QColor(120, 112, 102));
    painter.drawText(QRectF(rect.left(), plot.top() - 8, 28, 16),
                     Qt::AlignRight | Qt::AlignVCenter, QString::number(maximum, 'f', 0));
    painter.drawText(QRectF(rect.left(), plot.bottom() - 8, 28, 16),
                     Qt::AlignRight | Qt::AlignVCenter, "0");
}
