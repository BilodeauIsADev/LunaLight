#pragma once

#include "OrbitNavigation.h"

#include <algorithm>
#include <functional>
#include <initializer_list>
#include <vector>

namespace moonlight_xbox_dx {
namespace OrbitInput {

inline bool HasOpenPopup() {
	for (auto popup : Windows::UI::Xaml::Media::VisualTreeHelper::GetOpenPopups(Windows::UI::Xaml::Window::Current)) {
		if (popup->IsOpen) return true;
	}
	return false;
}

// Called during PreviewKeyDown, before GridView consumes the shoulders for paging.
inline bool HandleBumpers(Windows::UI::Xaml::Input::KeyRoutedEventArgs^ e, int activeTab,
	Windows::UI::Xaml::Controls::Button^ hosts,
	Windows::UI::Xaml::Controls::Button^ library,
	Windows::UI::Xaml::Controls::Button^ settings) {
	using namespace Windows::System;
	if (e->Key != VirtualKey::GamepadLeftShoulder && e->Key != VirtualKey::GamepadRightShoulder) return false;
	if (HasOpenPopup()) return false;
	e->Handled = true;
	if (e->KeyStatus.WasKeyDown) return true;
	Windows::UI::Xaml::Controls::Button^ tabs[] = { hosts, library, settings };
	bool available[3];
	for (int i = 0; i < 3; ++i) {
		available[i] = tabs[i] != nullptr && tabs[i]->IsEnabled && tabs[i]->Visibility == Windows::UI::Xaml::Visibility::Visible;
	}
	int currentTab = activeTab;
	auto focused = Windows::UI::Xaml::Input::FocusManager::GetFocusedElement();
	for (int i = 0; i < 3; ++i) {
		if (focused == tabs[i]) currentTab = i;
	}
	const int next = OrbitNavigation::NextTab(currentTab, e->Key == VirtualKey::GamepadLeftShoulder ? -1 : 1, available);
	if (next == currentTab) return true;
	auto target = tabs[next];
	target->Focus(Windows::UI::Xaml::FocusState::Keyboard);
	auto peer = ref new Windows::UI::Xaml::Automation::Peers::ButtonAutomationPeer(target);
	auto invoke = dynamic_cast<Windows::UI::Xaml::Automation::Provider::IInvokeProvider^>(
		peer->GetPattern(Windows::UI::Xaml::Automation::Peers::PatternInterface::Invoke));
	if (invoke != nullptr) invoke->Invoke();
	return true;
}

// Called during PreviewKeyDown. XY focus from the centred tab bar picks a
// neighbouring tab for Up and Down, so send Down into the page and keep Up here.
inline bool HandleTabBarVertical(Windows::UI::Xaml::Input::KeyRoutedEventArgs^ e,
	std::initializer_list<Windows::UI::Xaml::Controls::Button^> tabs,
	const std::function<Windows::UI::Xaml::Controls::Control^()>& below) {
	using namespace Windows::System;
	bool down = false;
	switch (e->Key) {
	case VirtualKey::GamepadDPadDown: case VirtualKey::GamepadLeftThumbstickDown: case VirtualKey::Down:
		down = true;
		break;
	case VirtualKey::GamepadDPadUp: case VirtualKey::GamepadLeftThumbstickUp: case VirtualKey::Up:
		break;
	default:
		return false;
	}
	if (HasOpenPopup()) return false;
	auto focused = Windows::UI::Xaml::Input::FocusManager::GetFocusedElement();
	bool onTab = false;
	for (auto tab : tabs) {
		if (tab != nullptr && focused == tab) onTab = true;
	}
	if (!onTab) return false;
	e->Handled = true;
	if (!down) return true;
	// Resolved only now: finding the target can scroll and realize the shelf
	auto target = below ? below() : nullptr;
	if (target != nullptr) target->Focus(Windows::UI::Xaml::FocusState::Keyboard);
	return true;
}

inline bool HasFocusIn(Windows::UI::Xaml::DependencyObject^ root) {
	auto node = dynamic_cast<Windows::UI::Xaml::DependencyObject^>(Windows::UI::Xaml::Input::FocusManager::GetFocusedElement());
	for (; node != nullptr; node = Windows::UI::Xaml::Media::VisualTreeHelper::GetParent(node)) {
		if (node == root) return true;
	}
	return false;
}

// Right and A on a section return to the row last used there. A row's XYFocusLeft is its section.
inline void RememberSectionRow(Platform::Object^ focused) {
	auto control = dynamic_cast<Windows::UI::Xaml::Controls::Control^>(focused);
	if (control == nullptr) return;
	auto section = dynamic_cast<Windows::UI::Xaml::Controls::Button^>(control->XYFocusLeft);
	if (section != nullptr) section->XYFocusRight = control;
}

// Visible, enabled controls under root, top to bottom
inline void CollectFocusable(Windows::UI::Xaml::DependencyObject^ root, std::vector<Windows::UI::Xaml::Controls::Control^>& out) {
	const int count = Windows::UI::Xaml::Media::VisualTreeHelper::GetChildrenCount(root);
	for (int i = 0; i < count; ++i) {
		auto child = Windows::UI::Xaml::Media::VisualTreeHelper::GetChild(root, i);
		auto element = dynamic_cast<Windows::UI::Xaml::FrameworkElement^>(child);
		if (element != nullptr && element->Visibility != Windows::UI::Xaml::Visibility::Visible) continue;
		auto control = dynamic_cast<Windows::UI::Xaml::Controls::Control^>(child);
		if (control != nullptr && control->IsTabStop) {
			if (control->IsEnabled) out.push_back(control);
			continue;
		}
		CollectFocusable(child, out);
	}
}

// Called during PreviewKeyDown. XY focus skips the right-aligned toggles in option
// rows, so walk the rows in order. Collected per key press: rows hide and disable.
inline bool HandleListVertical(Windows::UI::Xaml::Input::KeyRoutedEventArgs^ e, Windows::UI::Xaml::DependencyObject^ list) {
	using namespace Windows::System;
	int delta = 0;
	switch (e->Key) {
	case VirtualKey::GamepadDPadDown: case VirtualKey::GamepadLeftThumbstickDown: case VirtualKey::Down: delta = 1; break;
	case VirtualKey::GamepadDPadUp: case VirtualKey::GamepadLeftThumbstickUp: case VirtualKey::Up: delta = -1; break;
	// About a screenful of option rows
	case VirtualKey::GamepadRightTrigger: delta = 5; break;
	case VirtualKey::GamepadLeftTrigger: delta = -5; break;
	default: return false;
	}
	if (HasOpenPopup() || !HasFocusIn(list)) return false;
	std::vector<Windows::UI::Xaml::Controls::Control^> controls;
	CollectFocusable(list, controls);
	auto focused = Windows::UI::Xaml::Input::FocusManager::GetFocusedElement();
	for (int i = 0; i < static_cast<int>(controls.size()); ++i) {
		if (controls[i] != focused) continue;
		e->Handled = true;
		const int target = (std::max)(0, (std::min)(static_cast<int>(controls.size()) - 1, i + delta));
		if (target == i) return true;
		controls[target]->Focus(Windows::UI::Xaml::FocusState::Keyboard);
		// Show the whole row, not just its control: titles sit beside it
		Windows::UI::Xaml::DependencyObject^ node = controls[target];
		for (; node != nullptr; node = Windows::UI::Xaml::Media::VisualTreeHelper::GetParent(node)) {
			auto row = dynamic_cast<Windows::UI::Xaml::Controls::Border^>(node);
			if (row != nullptr) { row->StartBringIntoView(); break; }
		}
		return true;
	}
	return false;
}

inline bool HandleShelfTriggers(Windows::UI::Xaml::Input::KeyRoutedEventArgs^ e,
	Windows::UI::Xaml::Controls::GridView^ shelf, Platform::Object^ selectedItem) {
	using namespace Windows::System;
	if (e->Key != VirtualKey::GamepadLeftTrigger && e->Key != VirtualKey::GamepadRightTrigger) return false;
	if (HasOpenPopup()) return false;
	e->Handled = true;
	const int count = static_cast<int>(shelf->Items->Size);
	int current = -1;
	unsigned int index;
	if (selectedItem != nullptr && shelf->Items->IndexOf(selectedItem, &index)) current = static_cast<int>(index);
	int visibleCount = 1;
	auto panel = dynamic_cast<Windows::UI::Xaml::Controls::ItemsStackPanel^>(shelf->ItemsPanelRoot);
	if (panel != nullptr && panel->FirstVisibleIndex >= 0) {
		visibleCount = panel->LastVisibleIndex - panel->FirstVisibleIndex + 1;
	}
	const int next = OrbitNavigation::ShelfPageTarget(current, count, visibleCount,
		e->Key == VirtualKey::GamepadLeftTrigger ? -1 : 1);
	if (next < 0) return true;
	auto item = shelf->Items->GetAt(next);
	shelf->ScrollIntoView(item, Windows::UI::Xaml::Controls::ScrollIntoViewAlignment::Leading);
	// Realize the target before focusing it, including when it was virtualized.
	shelf->UpdateLayout();
	auto container = dynamic_cast<Windows::UI::Xaml::Controls::GridViewItem^>(shelf->ContainerFromItem(item));
	if (container != nullptr) container->Focus(Windows::UI::Xaml::FocusState::Keyboard);
	return true;
}

} // namespace OrbitInput
} // namespace moonlight_xbox_dx
