//
// MoonlightSettings.xaml.cpp
// Implementazione della classe MoonlightSettings
//

#include "pch.h"
#include "Common/OrbitInput.h"
#include "MoonlightSettings.xaml.h"
#include "Common/OrbitUiScale.h"
#include "Common/OrbitClock.h"
using namespace Windows::UI::Core;

using namespace moonlight_xbox_dx;

using namespace Platform;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;
using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Controls;
using namespace Windows::UI::Xaml::Controls::Primitives;
using namespace Windows::UI::Xaml::Data;
using namespace Windows::UI::Xaml::Input;
using namespace Windows::UI::Xaml::Media;
using namespace Windows::UI::Xaml::Navigation;

// Il modello di elemento Pagina vuota è documentato all'indirizzo https://go.microsoft.com/fwlink/?LinkId=234238

MoonlightSettings::MoonlightSettings()
{
	state = GetApplicationState();
	InitializeComponent();
	AttachUiScale(this, ScaledRoot);
	AttachClock(ClockText);
	GeneralSectionButton->XYFocusRight = AppSettings->SectionFocusTarget(0);
	DisplaySectionButton->XYFocusRight = AppSettings->SectionFocusTarget(1);
	InputSectionButton->XYFocusRight = AppSettings->SectionFocusTarget(2);
	AppSettings->ShowSection(0, GeneralSectionButton, CalibrationFrame);
	Windows::UI::ViewManagement::ApplicationView::GetForCurrentView()->SetDesiredBoundsMode(Windows::UI::ViewManagement::ApplicationViewBoundsMode::UseCoreWindow);
	Loaded += ref new RoutedEventHandler(this, &MoonlightSettings::OnLoaded);
	Unloaded += ref new RoutedEventHandler(this, &MoonlightSettings::OnUnloaded);
}

void MoonlightSettings::backButton_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
	GetApplicationState()->UpdateFile();
	if (Frame->CanGoBack) {
		while (Frame->BackStackDepth > 1) Frame->BackStack->RemoveAtEnd();
		Frame->GoBack();
	}
}

void MoonlightSettings::OnBackRequested(Platform::Object^ e, Windows::UI::Core::BackRequestedEventArgs^ args)
{
	// UWP on Xbox One triggers a back request whenever the B
	// button is pressed which can result in the app being
	// suspended if unhandled
	args->Handled = true;
	// B steps out of the options to the section list before leaving the page
	if (m_activeSection != nullptr && OrbitInput::HasFocusIn(AppSettings)) {
		m_activeSection->Focus(Windows::UI::Xaml::FocusState::Keyboard);
		return;
	}
	GetApplicationState()->UpdateFile();
	this->Frame->GoBack();
}

void MoonlightSettings::OnLoaded(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
	auto navigation = Windows::UI::Core::SystemNavigationManager::GetForCurrentView();
	m_back_cookie = navigation->BackRequested += ref new EventHandler<BackRequestedEventArgs^>(this, &MoonlightSettings::OnBackRequested);
	GeneralSectionButton->Focus(Windows::UI::Xaml::FocusState::Programmatic);
}

void MoonlightSettings::SectionButton_GotFocus(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
	auto selected = dynamic_cast<Button^>(sender);
	if (selected == nullptr) return;
	m_activeSection = selected;
	Button^ buttons[] = { GeneralSectionButton, DisplaySectionButton, InputSectionButton };
	Platform::String^ titles[] = { L"General", L"Display", L"Input" };
	for (int i = 0; i < 3; i++) {
		if (buttons[i] == selected) {
			for (int j = 0; j < 3; j++) {
				if (i == j) {
					buttons[j]->Background = safe_cast<Brush^>(Resources->Lookup("SettingsSectionSelectedBrush"));
					buttons[j]->Foreground = safe_cast<Brush^>(Application::Current->Resources->Lookup("OrbitTextBrush"));
				} else {
					buttons[j]->ClearValue(Control::BackgroundProperty);
					buttons[j]->ClearValue(Control::ForegroundProperty);
				}
			}
			SectionTitle->Text = titles[i];
			AppSettings->ShowSection(i, selected, CalibrationFrame);
			break;
		}
	}
}

// A on a section opens its options, as the Select hint promises
void MoonlightSettings::SectionButton_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
	SectionButton_GotFocus(sender, e);
	auto target = dynamic_cast<Control^>(safe_cast<Button^>(sender)->XYFocusRight);
	if (target != nullptr) target->Focus(Windows::UI::Xaml::FocusState::Keyboard);
}

void MoonlightSettings::OnUnloaded(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
	auto navigation = Windows::UI::Core::SystemNavigationManager::GetForCurrentView();
	navigation->BackRequested -= m_back_cookie;
}

void MoonlightSettings::Page_PreviewKeyDown(Platform::Object^ sender, Windows::UI::Xaml::Input::KeyRoutedEventArgs^ e) {
	if (OrbitInput::HandleBumpers(e, 2, backButton, nullptr, settingsNavButton)) return;
	if (OrbitInput::HandleTabBarVertical(e, { backButton, settingsNavButton }, [this]() -> Windows::UI::Xaml::Controls::Control^ { return m_activeSection; })) return;
	if (OrbitInput::HandleListVertical(e, AppSettings)) return;
}
