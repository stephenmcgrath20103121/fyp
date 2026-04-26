import { useMutation, useQueryClient } from '@tanstack/react-query';
import { updateMediaTitle } from '@/app/api/mediaroot-server-api';
import { Media } from '@/app/types/index';

type UpdateMediaTitleArgs = { id: number; title: string };

const useUpdateMediaTitle = () => {
  const queryClient = useQueryClient();
  return useMutation<Media, Error, UpdateMediaTitleArgs>({
    mutationFn: ({ id, title }) => updateMediaTitle(id, title),
    onSuccess: () => {
      queryClient.invalidateQueries({ queryKey: ['media'] });   //refresh library after update
    },
  });
};

export { useUpdateMediaTitle };