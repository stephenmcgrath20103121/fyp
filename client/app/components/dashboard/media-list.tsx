'use client'
import * as React from 'react';
import Typography from '@mui/material/Typography';
import Grid from '@mui/material/Grid';
import Pagination from '@mui/material/Pagination';
import Search from '@/app/components/dashboard/search';
import MediaCard from "@/app/components/dashboard/media-card";
import Spinner from '@/app/components/dashboard/spinner';
import { useMedia } from '@/app/hooks/useMedia';
import { MediaTypeFilter } from '@/app/page';

type Props = {
  selectedType: MediaTypeFilter;
};

const PAGE_SIZE = 20;

export default function MediaList({ selectedType }: Props) {
  const [searchQuery, setSearchQuery] = React.useState('');
  const [page, setPage] = React.useState(1);
  const { data, error, isLoading, isError }  = useMedia();

  //Resets to page 1 after searching
  React.useEffect(() => {
    setPage(1);
  }, [selectedType, searchQuery]);

  if (isLoading) { return <Spinner /> }
  if (isError) { return <Typography sx={{color: 'error.main'}}>{error.message}</Typography> }
  if (!data) { return null }

  const query = searchQuery.trim().toLowerCase();

  const filtered = data
    .filter((m) => m.media_type === selectedType)
    .filter((m) => query === '' || m.title.toLowerCase().includes(query));

  const pageCount = Math.max(1, Math.ceil(filtered.length / PAGE_SIZE));
  const safePage = Math.min(page, pageCount);
  const visible = filtered.slice((safePage - 1) * PAGE_SIZE, safePage * PAGE_SIZE);

  const mediaCards = visible.map((m) => (
    <MediaCard key={m.id} media={m} />
  ));

  return (
    <Grid sx={{ bgcolor: 'background.paper', pl: 1, pr: 1, pt: 1, pb: 2, borderRadius: 3}}>
      <Grid container columnSpacing={2} sx={{alignItems: 'center', mr: 1}}>
        <Typography component ="h5" variant="h5" gutterBottom sx={{pl:2, pt:2.5, pb:.5}}>
          Media
        </Typography>
        <Grid size='grow'>
          <Search value={searchQuery} onChange={setSearchQuery} />
        </Grid>
      </Grid>
      <Grid sx={{ bgcolor: 'tertiary.main', margin: 'auto', borderRadius: 2 }}>
        <Grid container sx={{justifyContent: "space-evenly", alignItems: "flex-start",}}>
          {mediaCards.length > 0 ? mediaCards : (
            <Typography sx={{ p: 4, color: 'text.primary' }}>
              {query
                ? `No ${selectedType} media matches "${searchQuery}".`
                : `No ${selectedType} media in your Library yet.`}
            </Typography>
          )}
        </Grid>
      </Grid>
      {pageCount > 1 && (
        <Grid container sx={{ justifyContent: 'center', mt: 2 }}>
          <Pagination
            count={pageCount}
            page={safePage}
            onChange={(_e, newPage) => {
              setPage(newPage)
              window.scrollTo({ top: 0, behavior: 'smooth' })
            }}
            color="secondary"
            shape="rounded"
            variant="outlined"
          />
        </Grid>
      )}
    </Grid>
  );
}