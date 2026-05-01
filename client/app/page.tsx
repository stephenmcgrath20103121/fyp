'use client';
import * as React from 'react';
import SideMenu from '@/app/components/dashboard/side-menu';
import MediaList from '@/app/components/dashboard/media-list';
import Options from '@/app/components/dashboard/options';
import Grid from '@mui/material/Grid';

export type MediaTypeFilter = 'video' | 'audio' | 'image';

export default function Home() {
  const [selectedType, setSelectedType] = React.useState<MediaTypeFilter>('video');

  return (
    <main>
      <Grid container spacing={3} sx={{ ml: 2, mr: 2, mb: 1, mt: 1}}>
        <Grid size={4} sx={{minWidth: 210}}>
          <Options />
          <SideMenu
            selectedType={selectedType}
            onSelectType={setSelectedType}
          />
        </Grid>
        <Grid size='grow' sx={{minWidth: 200}}>
          <MediaList selectedType={selectedType} />
        </Grid>
      </Grid>
    </main>
  );
}
