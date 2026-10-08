# Repository social preview

Upload [social-card.jpg](social-card.jpg) in the repository's **Settings → General
→ Social preview → Edit → Upload an image**. GitHub does not automatically select
an image merely because it is committed to the repository.

The finished card is a 2:1, opaque JPEG. It preserves the generated image's
1774 × 887 dimensions and stays below GitHub's 1 MB upload limit. GitHub recommends
1280 × 640 for best display; this card has the same aspect ratio at a larger size.
[Official instructions](https://docs.github.com/en/repositories/managing-your-repositorys-settings-and-features/customizing-your-repository/customizing-your-repositorys-social-media-preview).

## Provenance

Created with the built-in image-generation tool, using `docs/banner.png` as the
shoreline/style reference and `assets/title.png` as the wordmark reference.
The generated PNG was exported to JPEG at quality 94 without cropping, resizing,
or retouching, to fit the upload limit. This is generated promotional artwork,
not an unaltered gameplay capture. Original game imagery retains its original
rights; see [MEDIA.md](MEDIA.md).

## Generation prompt

```text
Use case: ads-marketing.
Asset type: GitHub repository social preview image, exact 1280 x 640 pixels, opaque PNG, landscape 2:1.
Create a polished social card for the existing Other Realm CHGame project.
Input image 1 is a reference for the game's actual shoreline scene, its limited blue/purple palette and flat polygon/pixel-art language. Input image 2 is the existing Other Realm wordmark to preserve faithfully, not a new logo to redesign.
Composition: extend the alien shoreline scene into a generous wide panorama. Preserve the tiny red-haired protagonist standing fully out of the water, on the ground in the right half. Preserve the rocky silhouettes, distant thin spires, cyan-blue sky, pale broken arcs and dark indigo foreground. Make it feel like the same game scene, with crisp flat color shapes and deliberate pixel stair-steps. No painterly texture, smooth 3D, photorealism, glow, or added characters.
Place the large two-line black-and-muted-gold angular wordmark in the upper left, using input image 2's exact letterforms and spelling "OTHER REALM", with comfortable 64-pixel edge margins. Keep strong contrast. Keep the title and the protagonist from overlapping.
On the lower left, set this exact existing tagline in restrained, readable ivory pixel-style lettering, on two lines:
"A whole other world."
"A very small handheld."
Below it, smaller uppercase text: "VECTOR ADVENTURE FOR CHGAME".
Use a naturally dark part of the landscape behind the small text, not a big UI panel. Keep the composition spacious and cinematic, title dominant, restrained typography. All important text at least 48 pixels from the canvas edges. No badge graphics, mock device, frame, watermark, URL, or extra words.
This is promotional illustrated artwork based on the actual game screenshots, not a screenshot that must be represented as unaltered. Save at exactly 1280x640.
```
