/*
Copyright (c) 2010-2014, Mathieu Labbe - IntRoLab - Universite de Sherbrooke
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the Universite de Sherbrooke nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "rtabmap/gui/CameraViewer.h"

#include <rtabmap/core/CameraEvent.h>
#include <rtabmap/core/util3d.h>
#include <rtabmap/core/util2d.h>
#include <rtabmap/gui/ImageView.h>
#include <rtabmap/gui/CloudViewer.h>
#include <rtabmap/utilite/UCv2Qt.h>
#include <rtabmap/utilite/ULogger.h>
#include <QtCore/QMetaType>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QSpinBox>
#include <QDialogButtonBox>

namespace rtabmap {


CameraViewer::CameraViewer(QWidget * parent) :
		QDialog(parent),
	imageView_(new ImageView(this)),
	cloudView_(new CloudViewer(this)),
	processingImages_(false),
	validDecimationValue_(1)
{
	qRegisterMetaType<rtabmap::SensorData>("rtabmap::SensorData");

	imageView_->setImageDepthShown(true);
	imageView_->setMinimumSize(320, 240);
	QHBoxLayout * layout = new QHBoxLayout();
	layout->setMargin(0);
	layout->addWidget(imageView_,1);
	layout->addWidget(cloudView_,1);

	QLabel * decimationLabel = new QLabel("Decimation", this);
	decimationSpin_ = new QSpinBox(this);
	decimationSpin_->setMinimum(1);
	decimationSpin_->setMaximum(16);
	decimationSpin_->setValue(1);

	QDialogButtonBox * buttonBox = new QDialogButtonBox(this);
	buttonBox->setStandardButtons(QDialogButtonBox::Close);
	connect(buttonBox, SIGNAL(rejected()), this, SLOT(reject()));

	QHBoxLayout * layout2 = new QHBoxLayout();
	layout2->addWidget(decimationLabel);
	layout2->addWidget(decimationSpin_);
	layout2->addStretch(1);
	layout2->addWidget(buttonBox);

	QVBoxLayout * vlayout = new QVBoxLayout(this);
	vlayout->setMargin(0);
	vlayout->setSpacing(0);
	vlayout->addLayout(layout, 1);
	vlayout->addLayout(layout2);

	this->setLayout(vlayout);
}

CameraViewer::~CameraViewer()
{
	this->unregisterFromEventsManager();
}

void CameraViewer::showImage(const rtabmap::SensorData & data)
{
	if(!data.imageRaw().empty() || !data.depthOrRightRaw().empty())
	{
		if(data.imageRaw().cols % decimationSpin_->value() == 0 &&
		   data.imageRaw().rows % decimationSpin_->value() == 0 &&
		   data.depthOrRightRaw().cols % decimationSpin_->value() == 0 &&
		   data.depthOrRightRaw().rows % decimationSpin_->value() == 0)
		{
			validDecimationValue_ = decimationSpin_->value();
		}
		else
		{
			UWARN("Decimation (%d) must be a denominator of the width and height of "
					"the image (color=%d/%d depth=%d/%d). Using last valid decimation value (%d).",
					decimationSpin_->value(),
					data.imageRaw().cols,
					data.imageRaw().rows,
					data.depthOrRightRaw().cols,
					data.depthOrRightRaw().rows,
					validDecimationValue_);
		}
	}

	processingImages_ = true;
	if(!data.imageRaw().empty())
	{
		imageView_->setImage(uCvMat2QImage(util2d::decimate(data.imageRaw(), validDecimationValue_)));
	}
	if(!data.depthOrRightRaw().empty())
	{
		imageView_->setImageDepth(uCvMat2QImage(util2d::decimate(data.depthOrRightRaw(), validDecimationValue_)));
	}
	if((data.stereoCameraModel().isValid() || (data.cameraModels().size() && data.cameraModels().at(0).isValid())))
	{
		if(!data.imageRaw().empty() && !data.depthOrRightRaw().empty())
		{
			cloudView_->addOrUpdateCloud("cloud", util3d::cloudRGBFromSensorData(data, validDecimationValue_));
			cloudView_->setVisible(true);
			cloudView_->update();
		}
		else if(!data.depthOrRightRaw().empty())
		{
			cloudView_->addOrUpdateCloud("cloud", util3d::cloudFromSensorData(data, validDecimationValue_));
			cloudView_->setVisible(true);
			cloudView_->update();
		}
	}
	else
	{
		cloudView_->setVisible(false);
	}
	processingImages_ = false;
}

void CameraViewer::handleEvent(UEvent * event)
{
	if(event->getClassName().compare("CameraEvent") == 0)
	{
		CameraEvent * camEvent = (CameraEvent*)event;
		if(camEvent->getCode() == CameraEvent::kCodeData)
		{
			if(camEvent->data().isValid())
			{
				if(!processingImages_ && this->isVisible() && camEvent->data().isValid())
				{
					processingImages_ = true;
					QMetaObject::invokeMethod(this, "showImage",
							Q_ARG(rtabmap::SensorData, camEvent->data()));
				}
			}
		}
	}
}

} /* namespace rtabmap */
