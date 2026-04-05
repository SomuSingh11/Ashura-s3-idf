#pragma once

// ============================================
// IScreen - Interface every screen must implement
//
// Lifecycle:
//   onEnter()  → called once when screen becomes active
//   update()   → called every loop() to redraw
//   onExit()   → called once when screen is popped
//
// Input (wire up to physical buttons):
//   onButtonUp()     → scroll up / previous item
//   onButtonDown()   → scroll down / next item
//   onButtonSelect() → confirm / enter
//   onButtonBack()   → UIManager.popScreen() is called automatically,
//                      override only if you need pre-exit logic
//
// For games and animations, override needsContinuousUpdate()
// to return true — update() will be called every loop tick
// regardless of the dirty flag.
// ============================================

class IScreen {
    public:
        virtual void onEnter()         {}
        virtual void onExit()          {}
        virtual void update()          = 0;

        virtual void onButtonUp()      {}
        virtual void onButtonDown()    {}
        virtual void onButtonSelect()  {}
        virtual void onButtonBack()    {}

        virtual void onLongPressUp()     {}
        virtual void onLongPressDown()   {}
        virtual void onLongPressSelect() {}
        virtual void onLongPressBack()   {}

        // Return true for games/animations that need update() every loop tick
        virtual bool needsContinuousUpdate() const { return false; }
        // Return true for screens that want to be popped (animation that self-closes, etc)
        virtual bool wantsPop() const { return false; }
        
        virtual bool isDirty()         { return _dirty; }
        virtual void markDirty()       { _dirty = true; }
        virtual ~IScreen()             = default;

    protected:
        bool _dirty = true; // start dirty so first frame draws
};