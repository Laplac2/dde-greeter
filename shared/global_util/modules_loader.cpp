/*
* Copyright (C) 2021 ~ 2021 Uniontech Software Technology Co.,Ltd.
*
* Author:     Zhang Qipeng <zhangqipeng@uniontech.com>
*
* Maintainer: Zhang Qipeng <zhangqipeng@uniontech.com>
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "modules_loader.h"

#include "base_module_interface.h"

#include <QDebug>
#include <QDir>
#include <QLibrary>
#include <QPluginLoader>

namespace dss {
namespace module {

const QString ModulesDir = "/usr/lib/dde-session-shell/modules";

ModulesLoader::ModulesLoader(QObject *parent)
    : QThread(parent)
{
}

ModulesLoader::~ModulesLoader()
{
}

ModulesLoader &ModulesLoader::instance()
{
    static ModulesLoader modulesLoader;
    return modulesLoader;
}

BaseModuleInterface *ModulesLoader::findModuleByName(const QString &name) const
{
    return m_modules.value(name, nullptr);
}

QHash<QString, BaseModuleInterface *> ModulesLoader::findModulesByType(const int type) const
{
    QHash<QString, BaseModuleInterface *> modules;
    for (BaseModuleInterface *module : m_modules.values()) {
        if (module->type() == type) {
            modules.insert(module->key(), module);
        }
    }
    return modules;
}

void ModulesLoader::run()
{
    findModule(ModulesDir);
}

bool ModulesLoader::checkVersion(const QString &target, const QString &base)
{
    if (target == base) {
        return true;
    }
    const QStringList baseVersion = base.split(".");
    const QStringList targetVersion = target.split(".");

    const int baseVersionSize = baseVersion.size();
    const int targetVersionSize = targetVersion.size();
    const int size = baseVersionSize < targetVersionSize ? baseVersionSize : targetVersionSize;

    for (int i = 0; i < size; i++) {
        if (targetVersion[i] == baseVersion[i]) continue;
        return targetVersion[i].toInt() > baseVersion[i].toInt() ? true : false;
    }
    return true;
}

void ModulesLoader::findModule(const QString &path)
{
    QDir dir(path);
    if (!dir.exists()) {
        qDebug() << path << "is not exists.";
        return;
    }
    const QFileInfoList modules = dir.entryInfoList();
    for (QFileInfo module : modules) {
        const QString path = module.absoluteFilePath();
        if (!QLibrary::isLibrary(path)) {
            continue;
        }
        qInfo() << module << "is found";
        QPluginLoader loader(path);
        const QJsonObject &meta = loader.metaData().value("MetaData").toObject();
        if (!checkVersion(meta.value("api").toString(), "1.0.0")) {
            qWarning() << "The module version is too low.";
            continue;
        }
        BaseModuleInterface *moduleInstance = dynamic_cast<BaseModuleInterface *>(loader.instance());
        if (!moduleInstance) {
            qWarning() << loader.errorString();
            continue;
        }
        if (m_modules.contains(moduleInstance->key())) {
            continue;
        }
        m_modules.insert(moduleInstance->key(), moduleInstance);
        emit moduleFound(moduleInstance);
    }
}

} // namespace module
} // namespace dss
