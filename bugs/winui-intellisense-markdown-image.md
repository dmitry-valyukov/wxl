# IntelliSense XML: a Markdown image in the description loses its URL and becomes "!alt text"

## Where it shows

`Microsoft.WindowsAppSDK.WinUI` 2.3.9 ships the API descriptions as IntelliSense files:
`metadata/Microsoft.UI.Xaml.xml` (beside `Microsoft.UI.Xaml.winmd`) and
`lib/net6.0-windows10.0.17763.0/Microsoft.WinUI.xml`. In both, three members carry an image
that their reference page shows, but the file keeps only an exclamation mark and the alt text:

| Member | Source page |
|---|---|
| `P:Microsoft.UI.Xaml.UIElement.KeyboardAccelerators` | `microsoft.ui.xaml/uielement_keyboardaccelerators.md` |
| `P:Microsoft.UI.Xaml.Controls.AppBarButton.KeyboardAcceleratorTextOverride` | `microsoft.ui.xaml.controls/appbarbutton_keyboardacceleratortextoverride.md` |
| `P:Microsoft.UI.Xaml.Controls.AppBarToggleButton.KeyboardAcceleratorTextOverride` | `microsoft.ui.xaml.controls/appbartogglebutton_keyboardacceleratortextoverride.md` |

## Source (MicrosoftDocs/winapps-winrt-api, branch `docs`)

`microsoft.ui.xaml/uielement_keyboardaccelerators.md`, section `-description`:

```markdown
Accelerators are typically assigned to buttons or menu items.

![Example of a menu showing keyboard accelerators for various menu items](images/keyboard-accelerators.png)<br/>
*Example of a menu showing keyboard accelerators for various menu items*
```

The image is published:
`https://learn.microsoft.com/windows/windows-app-sdk/api/winrt/microsoft.ui.xaml/images/keyboard-accelerators.png`
(HTTP 200, `image/png`), and the page
`https://learn.microsoft.com/windows/windows-app-sdk/api/winrt/microsoft.ui.xaml.uielement.keyboardaccelerators`
renders it.

## What the IntelliSense file has

`metadata/Microsoft.UI.Xaml.xml`, member `P:Microsoft.UI.Xaml.UIElement.KeyboardAccelerators`:

```xml
<summary>Gets the collection of key combinations that invoke an action using the keyboard.

Accelerators are typically assigned to buttons or menu items.

!Example of a menu showing keyboard accelerators for various menu items&lt;br/&gt;
Example of a menu showing keyboard accelerators for various menu items</summary>
```

- `![alt](path)` becomes `!alt`: the brackets and the path are dropped, so a tool that reads the
  file cannot show the image or link to it, and the reader sees a stray "!".
- The caption `*...*` loses its emphasis and reads as a repetition of the alt text.
- The `<br/>` stays, escaped. In the two `KeyboardAcceleratorTextOverride` members the source
  ends the image line with two spaces instead of `<br/>`, and those spaces stay too.

## Expected

The image survives the conversion with its URL, the way an HTML `<img>` in the source does.
`microsoft.ui.xaml.controls/symbol.md` writes its images as HTML:

```html
E8FB &nbsp;&nbsp;<img alt="Accept icon" src="images/segoe-fluent-icons/e8fb.png" />
```

and the same file keeps them with the path rewritten relative to the API root:

```xml
<summary>E8FB   &lt;img alt="Accept icon" src="./microsoft.ui.xaml.controls/images/segoe-fluent-icons/e8fb.png" /&gt;</summary>
```

A Markdown image converted the same way (an `<img>` with the rewritten `src` and the alt text),
or at least kept as `![alt](./microsoft.ui.xaml/images/keyboard-accelerators.png)`, would keep the
description usable outside learn.microsoft.com: in IntelliSense, in object browsers and in
documentation generated from the package.

## How it was found

A documentation generator that reads these files to put the descriptions into its own API
reference. Searching the IntelliSense files of `Microsoft.WindowsAppSDK.WinUI` 2.3.9,
`.InteractiveExperiences` 2.1.9 and `.Foundation` 2.3.12 found the pattern only in these three
members.
