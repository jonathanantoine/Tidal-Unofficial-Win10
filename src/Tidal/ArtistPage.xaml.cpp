﻿//
// ArtistPage.xaml.cpp
// Implementation of the ArtistPage class
//

#include "pch.h"
#include "ArtistPage.xaml.h"
#include "AnimationHelpers.h"
#include "tools/TimeUtils.h"
#include <WindowsNumerics.h>
#include "ArtistDataSources.h"
#include <Api/ImageUriResolver.h>
#include <GroupedAlbums.h>
#include "IncrementalDataSources.h"
#include <Api/ApiErrors.h>
#include <Shell.xaml.h>
#include "AlbumPage.xaml.h"
#include "VideoItemVM.h"
using namespace Tidal;

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

// The Blank Page item template is documented at http://go.microsoft.com/fwlink/?LinkId=234238

ArtistPage::ArtistPage()
{
	InitializeComponent();
}

void parseBio(TextBlock^ txtBlock, std::wstring bioText) {
	txtBlock->Inlines->Clear();
	auto pos = bioText.find(L"<br/>", 0);
	while (pos != bioText.npos) {
		bioText.replace(pos, 5, L"\n");
		pos = bioText.find(L"<br/>", 0);
	}
	pos = bioText.find(L"  ", 0);
	while (pos != bioText.npos) {
		bioText.replace(pos, 5, L" ");
		pos = bioText.find(L"  ", 0);
	}
	pos = 0;
	auto wimplinkPos = bioText.find(L"[wimpLink", pos);
	while (wimplinkPos != bioText.npos) {
		auto beforeLinkRun = ref new Windows::UI::Xaml::Documents::Run();
		beforeLinkRun->Text = ref new String(&bioText[pos], wimplinkPos - pos);
		txtBlock->Inlines->Append(beforeLinkRun);
		
		auto textStart = bioText.find(L']', wimplinkPos)+1;
		auto textEnd = bioText.find(L"[/wimpLink]", textStart);
		
		auto link = ref new Windows::UI::Xaml::Documents::Hyperlink();
		auto linkText = ref new Windows::UI::Xaml::Documents::Run();
		linkText->Text = ref new String(&bioText[textStart], textEnd - textStart);
		link->Inlines->Append(linkText);

		if (bioText.find(L"artistId=\"", wimplinkPos+10) == wimplinkPos + 10) {
			wchar_t* unused;
			std::int64_t id = std::wcstoll(&bioText[wimplinkPos + 10 + 10], &unused, 10);
			link->Click += ref new Windows::Foundation::TypedEventHandler<Windows::UI::Xaml::Documents::Hyperlink ^, Windows::UI::Xaml::Documents::HyperlinkClickEventArgs ^>([id](Windows::UI::Xaml::Documents::Hyperlink ^, Windows::UI::Xaml::Documents::HyperlinkClickEventArgs ^) {
				auto shell = dynamic_cast<Shell^>(Windows::UI::Xaml::Window::Current->Content);
				if (shell) {
					shell->Frame->Navigate(ArtistPage::typeid, id);
				}
			});
		}
		else if (bioText.find(L"albumId=\"", wimplinkPos + 10) == wimplinkPos + 10) {
			auto indexOfIdEnd = bioText.find(L'\"', wimplinkPos + 10 + 9);
			auto id = ref new String(&bioText[wimplinkPos + 10 + 9], indexOfIdEnd - (wimplinkPos + 10 + 9));
			link->Click += ref new Windows::Foundation::TypedEventHandler<Windows::UI::Xaml::Documents::Hyperlink ^, Windows::UI::Xaml::Documents::HyperlinkClickEventArgs ^>([id](Windows::UI::Xaml::Documents::Hyperlink ^, Windows::UI::Xaml::Documents::HyperlinkClickEventArgs ^) {
				auto shell = dynamic_cast<Shell^>(Windows::UI::Xaml::Window::Current->Content);
				if (shell) {
					shell->Frame->Navigate(AlbumPage::typeid, id);
				}
			});
		}
		txtBlock->Inlines->Append(link);

		pos = textEnd + 11;
		wimplinkPos = bioText.find(L"[wimpLink", pos);
	}

	if (pos != bioText.size() && pos != bioText.npos) {
		auto lastRun = ref new Windows::UI::Xaml::Documents::Run();
		lastRun->Text = ref new String(&bioText[pos]);
		txtBlock->Inlines->Append(lastRun);
	}

}

concurrency::task<void> Tidal::ArtistPage::LoadAsync(Windows::UI::Xaml::Navigation::NavigationEventArgs ^ args)
{
	auto id = dynamic_cast<IBox<std::int64_t>^>(args->Parameter)->Value;
	auto info = await artists::getArtistInfoAsync(id, concurrency::cancellation_token::none());
	videosGV->ItemsSource = getArtistVideosDataSource(id);
	similarArtistsGV->ItemsSource = getSimilarArtistsDataSource(id);
	auto popularTracksTask = artists::getArtistPopularTracksAsync(id, 10, concurrency::cancellation_token::none());
	auto albumsTask = artists::getArtistAlbumsAsync(id, concurrency::cancellation_token::none());
	auto singlesTask = artists::getArtistAlbumsAsync(id, concurrency::cancellation_token::none(), L"EPSANDSINGLES");
	auto compilationsTask = artists::getArtistAlbumsAsync(id, concurrency::cancellation_token::none(), L"COMPILATIONS");
	auto bioTask = artists::getArtistBioAsync(id, concurrency::cancellation_token::none());
	if (info->picture.size() > 0) {
		loadImageAsync(api::resolveImageUri(info->picture, 1024, 256));
	}
	pageHeader->Text = tools::strings::toWindowsString(info->name);
	headerArtistName->Text = tools::strings::toWindowsString(info->name);
	headerImageRound->ImageSource = ref new Windows::UI::Xaml::Media::Imaging::BitmapImage(ref new Uri(api::resolveImageUri(info->picture, 160, 160)));
	auto tracks = await popularTracksTask;
	auto trackSource = ref new Platform::Collections::Vector<TrackItemVM^>();
	for (auto&& t : tracks->items) {
		trackSource->Append(ref new TrackItemVM(t));
	}
	popularTracksLV->ItemsSource = trackSource;
	auto albumGroups = ref new Platform::Collections::Vector<GroupedAlbums^>();
	{
		auto albums = ref new GroupedAlbums();
		albums->Title = L"ALBUMS";
		albums->Albums = ref new Platform::Collections::Vector<AlbumResumeItemVM^>();
		auto albumsSource = await albumsTask;
		for (auto&& a : albumsSource->items) {
			albums->Albums->Append(ref new AlbumResumeItemVM(a));
		}
		albumGroups->Append(albums);
	}

	{
		auto albums = ref new GroupedAlbums();
		albums->Title = L"SINGLES";
		albums->Albums = ref new Platform::Collections::Vector<AlbumResumeItemVM^>();
		auto albumsSource = await singlesTask;
		for (auto&& a : albumsSource->items) {
			albums->Albums->Append(ref new AlbumResumeItemVM(a));
		}
		albumGroups->Append(albums);
	}
	{
		auto albums = ref new GroupedAlbums();
		albums->Title = L"COMPILATIONS";
		albums->Albums = ref new Platform::Collections::Vector<AlbumResumeItemVM^>();
		auto albumsSource = await compilationsTask;
		for (auto&& a : albumsSource->items) {
			albums->Albums->Append(ref new AlbumResumeItemVM(a));
		}
		albumGroups->Append(albums);
	}

	auto viewSource = ref new CollectionViewSource();
	viewSource->Source = albumGroups;
	viewSource->IsSourceGrouped = true;
	viewSource->ItemsPath = ref new PropertyPath( L"Albums");
	discoGridView->ItemsSource = viewSource->View;
	try {
		auto bio = await bioTask;
		parseBio(bioTxt, bio->text);
		bioSource->Text = L"Source: " + tools::strings::toWindowsString(bio->source);
	}
	catch (api::statuscode_exception& ex) {
		if (ex.getStatusCode() == Windows::Web::Http::HttpStatusCode::NotFound) {
			unsigned int index;
			if (pivot->Items->IndexOf(bioPivotItem, &index)) {
				pivot->Items->RemoveAt(index);
			}
		}
		else {
			throw;
		}
	}
}



concurrency::task<void> Tidal::ArtistPage::loadImageAsync(Platform::String ^ url)
{
	headerImage->CustomDevice = Microsoft::Graphics::Canvas::CanvasDevice::GetSharedDevice();
	auto bmp = await Microsoft::Graphics::Canvas::CanvasBitmap::LoadAsync(Microsoft::Graphics::Canvas::CanvasDevice::GetSharedDevice(), ref new Uri(url));

	_albumBmp = bmp;
	await Dispatcher->RunAsync(Windows::UI::Core::CoreDispatcherPriority::Normal, ref new Windows::UI::Core::DispatchedHandler([this]() {
		headerImage->Opacity = 0;
		headerImage->Invalidate();
		AnimateTo(headerImage, L"Opacity", 1.0, tools::time::ToWindowsTimeSpan(std::chrono::milliseconds(150)));
	}));
}

void Tidal::ArtistPage::OnDrawHeaderImage(Microsoft::Graphics::Canvas::UI::Xaml::ICanvasAnimatedControl^ sender, Microsoft::Graphics::Canvas::UI::Xaml::CanvasAnimatedDrawEventArgs^ args)
{
	if (_albumBmp) {
		auto blur = ref new Microsoft::Graphics::Canvas::Effects::GaussianBlurEffect();
		blur->BlurAmount = 10;
		blur->Source = _albumBmp;
		blur->BorderMode = Microsoft::Graphics::Canvas::Effects::EffectBorderMode::Hard;

		
		auto session = args->DrawingSession;
		session->Clear(Windows::UI::Colors::Black);
		auto targetSize = sender->Size;
		auto srcSize = _albumBmp->GetBounds(session);
		auto targetAspectRatio = targetSize.Width / targetSize.Height;
		auto srcAspectRatio = srcSize.Width / srcSize.Height;
		auto opacityBrush = ref new Microsoft::Graphics::Canvas::Brushes::CanvasLinearGradientBrush(sender, Windows::UI::ColorHelper::FromArgb(192, 0, 0, 0), Windows::UI::ColorHelper::FromArgb(0, 0, 0, 0) );
		opacityBrush->StartPoint = Windows::Foundation::Numerics::float2(0, 0);
		opacityBrush->EndPoint = Windows::Foundation::Numerics::float2(0, targetSize.Height);
		if (targetAspectRatio > srcAspectRatio) {
			auto h = srcSize.Width / targetAspectRatio;
			auto blurBrush = ref new Microsoft::Graphics::Canvas::Brushes::CanvasImageBrush(sender, blur);
			
			blurBrush->SourceRectangle = Rect(0, (srcSize.Height - h)*.5f, srcSize.Width, h);

			auto scale = targetSize.Width / blurBrush->SourceRectangle->Value.Width;
			blurBrush->Transform = Windows::Foundation::Numerics::make_float3x2_scale(scale);
			session->FillRectangle(Rect(0, 0, targetSize.Width, targetSize.Height), blurBrush, opacityBrush);
		}
		else {
			auto w = srcSize.Height*targetAspectRatio;

			auto blurBrush = ref new Microsoft::Graphics::Canvas::Brushes::CanvasImageBrush(sender, blur);
			blurBrush->SourceRectangle = Rect((srcSize.Width - w)*.5f, 0, w, srcSize.Height);

			auto scale = targetSize.Width / blurBrush->SourceRectangle->Value.Width;
			blurBrush->Transform = Windows::Foundation::Numerics::make_float3x2_scale(scale);
			session->FillRectangle(Rect(0, 0, targetSize.Width, targetSize.Height), blurBrush, opacityBrush);
		}
	}
}


void Tidal::ArtistPage::OnPlayFromTrack(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{

}


void Tidal::ArtistPage::OnPauseFromTrack(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{

}


void Tidal::ArtistPage::OnAlbumClick(Platform::Object^ sender, Windows::UI::Xaml::Controls::ItemClickEventArgs^ e)
{
	auto item = dynamic_cast<AlbumResumeItemVM^>(e->ClickedItem);
	if (item) {
		item->Go();
	}
}


void Tidal::ArtistPage::OnVideoClicked(Platform::Object^ sender, Windows::UI::Xaml::Controls::ItemClickEventArgs^ e)
{
	auto item = dynamic_cast<VideoItemVM^>(e->ClickedItem);
	if (item) {
		item->Go();
	}
}


void Tidal::ArtistPage::OnSimilarArtistClicked(Platform::Object^ sender, Windows::UI::Xaml::Controls::ItemClickEventArgs^ e)
{
	auto item = dynamic_cast<ArtistItemVM^>(e->ClickedItem);
	if (item) {
		item->Go();
	}
}
