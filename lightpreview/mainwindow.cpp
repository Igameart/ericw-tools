/*  Copyright (C) 2017 Eric Wasylishen

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA

See file, 'COPYING', for details.
*/

#include "mainwindow.h"

#include <QDragEnterEvent>
#include <QMimeData>
#include <QFileSystemWatcher>
#include <QFileInfo>
#include <QFormLayout>
#include <QLineEdit>
#include <QSplitter>
#include <QCheckBox>

#include <common/bspfile.hh>
#include <qbsp/qbsp.hh>
#include <vis/vis.hh>
#include <light/light.hh>

#include "glview.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    resize(640, 480);

    // gl view
    glView = new GLView();

    // properties form
    auto *formLayout = new QFormLayout();

    vis_checkbox = new QCheckBox(tr("vis"));

    qbsp_options = new QLineEdit();
    vis_options = new QLineEdit();
    light_options = new QLineEdit();

    formLayout->addRow(tr("qbsp"), qbsp_options);
    formLayout->addRow(vis_checkbox, vis_options);
    formLayout->addRow(tr("light"), light_options);

    auto *form = new QWidget();
    form->setLayout(formLayout);

    // splitter

    auto *splitter = new QSplitter();
    splitter->addWidget(form);
    splitter->addWidget(glView);

    setCentralWidget(splitter);
    setAcceptDrops(true);
}

MainWindow::~MainWindow() { }

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls())
        event->acceptProposedAction();
}

void MainWindow::dropEvent(QDropEvent *event)
{
    auto urls = event->mimeData()->urls();
    if (!urls.empty()) {
        const QUrl &url = urls[0];
        if (url.isLocalFile()) {
            loadFile(url.toLocalFile());

            event->acceptProposedAction();
        }
    }
}

void MainWindow::loadFile(const QString &file)
{
    qDebug() << "load " << file;

    if (m_watcher) {
        delete m_watcher;
    }
    m_watcher = new QFileSystemWatcher(this);

    // start watching it
    qDebug() << "adding path: " << m_watcher->addPath(file);
    connect(m_watcher, &QFileSystemWatcher::fileChanged, this, [&](const QString &path) {
        if (QFileInfo(path).size() == 0) {
            // saving a map in TB produces 2 change notifications on Windows; the
            // first truncates the file to 0 bytes, so ignore that.
            return;
        }
        qDebug() << "got change notif for " << path;
        loadFileInternal(path);
    });

    loadFileInternal(file);
}

std::filesystem::path MakeFSPath(const QString &string)
{
    return std::filesystem::path{string.toStdU16String()};
}

static bspdata_t QbspVisLight_Common(const std::filesystem::path &name, std::vector<std::string> extra_qbsp_args,
    std::vector<std::string> extra_vis_args, std::vector<std::string> extra_light_args, bool run_vis)
{
    auto bsp_path = name;
    bsp_path.replace_extension(".bsp");

    std::vector<std::string> args{
        "", // the exe path, which we're ignoring in this case
    };
    for (auto &extra : extra_qbsp_args) {
        args.push_back(extra);
    }
    args.push_back(name.string());

    // run qbsp

    InitQBSP(args);
    ProcessFile();

    // run vis
    if (run_vis) {
        std::vector<std::string> vis_args{
            "", // the exe path, which we're ignoring in this case
        };
        for (auto &extra : extra_vis_args) {
            vis_args.push_back(extra);
        }
        vis_args.push_back(name.string());
        vis_main(vis_args);
    }

    // run light
    {
        std::vector<std::string> light_args{
            "", // the exe path, which we're ignoring in this case
        };
        for (auto &arg : extra_light_args) {
            light_args.push_back(arg);
        }
        light_args.push_back(name.string());

        light_main(light_args);
    }

    // serialize obj
    {
        bspdata_t bspdata;
        LoadBSPFile(bsp_path, &bspdata);

        ConvertBSPFormat(&bspdata, &bspver_generic);

        return std::move(bspdata);
    }
}

static std::vector<std::string> ParseArgs(const QLineEdit *line_edit)
{
    std::vector<std::string> result;

    QString text = line_edit->text().trimmed();
    if (text.isEmpty())
        return result;

    for (const auto &str : text.split(' ')) {
        qDebug() << "got token " << str;
        result.push_back(str.toStdString());
    }

    return result;
}

void MainWindow::loadFileInternal(const QString &file)
{
    qDebug() << "loadFileInternal " << file;

    auto d = QbspVisLight_Common(MakeFSPath(file), ParseArgs(qbsp_options), ParseArgs(vis_options),
        ParseArgs(light_options), vis_checkbox->isChecked());

    const auto &bsp = std::get<mbsp_t>(d.bsp);

    glView->renderBSP(file, bsp);
}