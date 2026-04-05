#pragma once
#include <vector>
#include "../ui/screens/IScreen.h"
#include "DisplayManager.h"

// ============================================================
// UIManager - Owns the screen stack
//      UIManager  —  Screen stack + ViewDispatcher bridge
//      Owns the push/pop stack for simple navigation.
//      For complex apps (Settings, etc.) compose with
//      ViewDispatcher + SceneManager instead.
//
// Navigation:
//   pushScreen(new SomeScreen()) → go forward
//   popScreen()                  → go back (auto-frees memory)
//
// Continuous update:
//   Screens that override needsContinuousUpdate() → true
//   get update() called every loop tick regardless of dirty flag.
//   This powers games and animations.
//
// WantsPop pattern:
//   Animations/splash screens can't call popScreen() themselves.
//   UIManager checks wantsPop() on supported screens each loop.
// =============================================================



class UIManager {
    public:
        void init(DisplayManager* display) { _display = display; };

        // ===== Navigation =====
        void pushScreen(IScreen* screen) {
            if(currentScreen()) currentScreen()->onExit();
            _stack.push_back(screen);
            screen->onEnter();
            screen->markDirty();
        };

        void popScreen() {
            if(_stack.size() <= 1) return;
            currentScreen()->onExit();
            delete _stack.back();
            _stack.pop_back();
            currentScreen()->onEnter();
            currentScreen()->markDirty();
        };

        // ===== Retrieval (pointer to the current) =====
        IScreen* currentScreen() {
            if(_stack.empty()) return nullptr;
            return _stack.back();
        }

        // ===== Update / Render =====
        void update() {
            IScreen* screen = currentScreen();
            if(!screen) return;

            if(screen->needsContinuousUpdate()){
                screen->update();
                if(screen->wantsPop()) popScreen();
            } else if (screen->isDirty()){
                screen->update();
            }

            if(screen->wantsPop()) popScreen(); 
        }

        // ===== Input Forwarding =====
        void onButtonUp() { if(currentScreen()) currentScreen()->onButtonUp(); }
        void onButtonDown() { if (currentScreen()) currentScreen()->onButtonDown(); }
        void onButtonSelect() { if (currentScreen()) currentScreen()->onButtonSelect(); }
        void onButtonBack() {
            if(currentScreen()) currentScreen()->onButtonBack();
            popScreen();
        };

        void onLongPressUp() { if(currentScreen()) currentScreen()->onLongPressUp(); }
        void onLongPressDown() { if(currentScreen()) currentScreen()->onLongPressDown(); }
        void onLongPressSelect() { if(currentScreen()) currentScreen()->onLongPressSelect(); }
        void onLongPressBack() { if(currentScreen()) currentScreen()->onLongPressBack(); };      

        size_t stackDepth() const { return _stack.size(); }

    private: 
        DisplayManager*         _display = nullptr; // Pointer to the display system.
        std::vector<IScreen*>   _stack;             // stack of screens
};