/***************************************************************************
 *   Copyright (C) 2019 by Jean-Baptiste Mardelle                          *
 *   This file is part of Kdenlive. See www.kdenlive.org.                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) version 3 or any later version accepted by the       *
 *   membership of KDE e.V. (or its successor approved  by the membership  *
 *   of KDE e.V.), which shall act as a proxy defined in Section 14 of     *
 *   version 3 of the license.                                             *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 ***************************************************************************/

#include "cachejob.hpp"
#include "bin/projectclip.h"
#include "bin/projectitemmodel.h"
#include "bin/projectsubclip.h"
#include "core.h"
#include "doc/kthumb.h"
#include "klocalizedstring.h"
#include "macros.hpp"
#include "utils/thumbnailcache.hpp"
#include <QImage>
#include <QScopedPointer>
#include <mlt++/MltProducer.h>

CacheJob::CacheJob(const QString &binId, int imageHeight, std::set<int> frames)
    : AbstractClipJob(CACHEJOB, binId)
    , m_frames(frames)
    , m_fullWidth(imageHeight * pCore->getCurrentDar() + 0.5)
    , m_imageHeight(imageHeight)
    , m_done(false)

{
    if (m_fullWidth % 8 > 0) {
        m_fullWidth += 8 - m_fullWidth % 8;
    }
    m_imageHeight += m_imageHeight % 2;
    auto item = pCore->projectItemModel()->getItemByBinId(binId);
    Q_ASSERT(item->itemType() == AbstractProjectItem::ClipItem);
}

const QString CacheJob::getDescription() const
{
    return i18n("Extracting thumbs from clip %1", m_clipId);
}

bool CacheJob::startJob()
{
    // We reload here, because things may have changed since creation of this job
    m_binClip = pCore->projectItemModel()->getClipByBinID(m_clipId);
    if (m_binClip->clipType() != ClipType::Video && m_binClip->clipType() != ClipType::AV) {
        // Don't create thumbnail for audio clips
        m_done = false;
        return true;
    }
    m_prod = m_binClip->thumbProducer();
    if ((m_prod == nullptr) || !m_prod->is_valid()) {
        qDebug() << "********\nCOULD NOT READ THUMB PRODUCER\n********";
        return false;
    }
    for (int i : m_frames) {
        if (ThumbnailCache::get()->hasThumbnail(m_binClip->clipId(), i)) {
            continue;
        }
        m_prod->seek(i);
        QScopedPointer<Mlt::Frame> frame(m_prod->get_frame());
        frame->set("deinterlace_method", "onefield");
        frame->set("top_field_first", -1);
        frame->set("rescale.interp", "nearest");
        if ((frame != nullptr) && frame->is_valid()) {
            QImage result = KThumb::getFrame(frame.data());
            ThumbnailCache::get()->storeThumbnail(m_binClip->clipId(), i, result, true);
        }
    }
    m_done = true;
    return true;
}

bool CacheJob::commitResult(Fun &undo, Fun &redo)
{
    Q_ASSERT(!m_resultConsumed);
    m_resultConsumed = true;
    return m_done;
}