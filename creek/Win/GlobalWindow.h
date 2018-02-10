#ifndef GLOBALWINDOW_H
#define GLOBALWINDOW_H

/**
 * GlobalWindow.h
 * 
 * Created on: June 24, 2016
 *     Author: hym
 */

/**
 * The default window into which all data is placed (via {@link GlobalWindows}).
 */
class GlobalWindow : BoundedWindow{
public:

	/**
	 * Singleton instance of {@link GlobalWindow}.
	 */
	//GlobalWindow(){}
        static GlobalWindow INSTANCE;

 
};

#endif /* GLOBALWINDOW_H */
