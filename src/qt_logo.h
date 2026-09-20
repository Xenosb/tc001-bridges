/*
 * Copyright (c) 2026 Bruno Vunderl
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef TC001_QT_LOGO_H_
#define TC001_QT_LOGO_H_

#define QT_LOGO_W 11
#define QT_LOGO_H 8

/*
 * The Qt badge as a mask: 'X' is a lit pixel, anything else is off. It is drawn in whatever
 * colour the caller likes. The "Qt" lettering is cut out of the badge, as in the real logo.
 */
extern const char *const qt_logo[QT_LOGO_H];

#endif /* TC001_QT_LOGO_H_ */
