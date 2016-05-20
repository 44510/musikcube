#pragma once

#include "ILayout.h"
#include "ILayoutStack.h"

#include <memory>

class LayoutStack : public ILayout, public ILayoutStack {
    public:
        LayoutStack();
        virtual ~LayoutStack();

        /* ILayout */
        virtual IWindowPtr FocusNext();
        virtual IWindowPtr FocusPrev();
        virtual IWindowPtr GetFocus();
        virtual ILayoutStack* GetLayoutStack();
        virtual void SetLayoutStack(ILayoutStack* stack);
        virtual void Layout() { }

        /* IOrderable */
        virtual void BringToTop();
        virtual void SendToBottom();

        /* IDisplayable */
        virtual void Show();
        virtual void Hide();

        /* IKeyHandler */
        virtual bool KeyPress(int64 ch);

        /* IWindowGroup */
        virtual bool AddWindow(IWindowPtr window);
        virtual bool RemoveWindow(IWindowPtr window);
        virtual size_t GetWindowCount();
        virtual IWindowPtr GetWindowAt(size_t position);

        /* ILayoutStack */
        virtual bool Push(ILayoutPtr layout);
        virtual bool Pop(ILayoutPtr layout);
        virtual bool BringToTop(ILayoutPtr layout);
        virtual bool SendToBottom(ILayoutPtr layout);

    private:
        std::deque<ILayoutPtr> layouts;
        ILayoutStack* stack;
        bool visible;
};